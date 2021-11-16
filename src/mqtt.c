/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <drivers/uart.h>
#include <random/rand32.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <modem/at_cmd.h>
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <logging/log.h>
#include <data/jwt.h>
#include <date_time.h>

#include "mqtt.h"
#include "certificates.h"
#include "keys.h"
#include "gps_conn.h"

#define PRIMARY_SEC_TAG 10
#define BACKUP_SEC_TAG  11



K_SEM_DEFINE(time_sem, 0, 1);
K_MSGQ_DEFINE(mqtt_msgq, sizeof(mqtt_msg_type), 10, 4);


// Buffers for MQTT client
static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];
static uint8_t jwt_buf[256];

// MQTT client context
static struct mqtt_client client_ctx;

// MQTT Broker address info
static struct sockaddr_storage broker;

// File descrciptor
static struct pollfd fds;


static int certificates_provision(void) {
    int err = 0;
    printk("Provisioning certificates\n");
    err = modem_key_mgmt_write(PRIMARY_SEC_TAG,
                    MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
                    PRIMARY_CA,
                    strlen(PRIMARY_CA));
    if (err) {
        printk("Failed to provision primary CA certificate: %d", err);
        return err;
    }

    err = modem_key_mgmt_write(BACKUP_SEC_TAG,
                    MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
                    BACKUP_CA,
                    strlen(BACKUP_CA));
    if (err) {
        printk("Failed to provision backup CA certificate: %d", err);
        return err;
    }

    return err;
}


int publish(uint8_t *data, size_t len) {
    struct mqtt_publish_param param;

    param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
    param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
    param.message.payload.data = data;
    param.message.payload.len = len;
    param.message_id = sys_rand32_get();
    param.dup_flag = 0;
    param.retain_flag = 0;

    return mqtt_publish(&client_ctx, &param);
}


static int subscribe(void) {
    struct mqtt_topic subscribe_topic = {
        .topic = {
            .utf8 = CONFIG_MQTT_SUB_TOPIC,
            .size = strlen(CONFIG_MQTT_SUB_TOPIC)
        },
        .qos = MQTT_QOS_1_AT_LEAST_ONCE
    };

    const struct mqtt_subscription_list subscription_list = {
        .list = &subscribe_topic,
        .list_count = 1,
        .message_id = 1234
    };

    printk("Subscribing to %s\n", CONFIG_MQTT_SUB_TOPIC);

    return mqtt_subscribe(&client_ctx, &subscription_list);
}


static int publish_get_payload(struct mqtt_client *client, size_t length) {
    if (length > sizeof(payload_buf)) {
        return -EMSGSIZE;
    }

    return mqtt_readall_publish_payload(client, payload_buf, length);
}



void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt) {

    int err;

    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0) {
            printk("MQTT connection failed %d\n", evt->result);
            break;
        }
        printk("MQTT client connected!\n");
        subscribe();
        break;
        
    case MQTT_EVT_DISCONNECT:
        printk("MQTT client disconnected %d\n", evt->result);
        break;

    case MQTT_EVT_PUBLISH: {
        const struct mqtt_publish_param *p = &evt->param.publish;
        printk("MQTT PUBLISH result: %d\n", evt->result);
        err = publish_get_payload(client, p->message.payload.len);

        if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
            const struct mqtt_puback_param ack = {
                .message_id = p->message_id
            };

            /* Send acknowledgement */
            mqtt_publish_qos1_ack(&client_ctx, &ack);
        }

        if (err >= 0) {
            payload_buf[p->message.payload.len] = '\0';
            printk("Data received: %s\n", payload_buf);
            /* Echo back received data */
            publish(payload_buf, p->message.payload.len);
        } else {
            printk("publish_get_payload failed: %d\n", err);
            printk("Disconnecting MQTT client...\n");

            err = mqtt_disconnect(client);
            if (err) {
                printk("Could not disconnect: %d\n", err);
            }
        }
    } break;

    case MQTT_EVT_PUBACK:
        if (evt->result != 0) {
            printk("MQTT PUBACK error %d\n", evt->result);
            break;
        }
        printk("PUBACK packet id: %u", evt->param.puback.message_id);
        break;

    case MQTT_EVT_PUBREC:
        if (evt->result != 0) {
            printk("MQTT PUBREC error %d\n", evt->result);
            break;
        }
        printk("PUBREC packet id: %u", evt->param.pubrec.message_id);

        const struct mqtt_pubrel_param rel_param = {
            .message_id = evt->param.pubrec.message_id
        };

        err = mqtt_publish_qos2_release(client, &rel_param);
        if (err != 0) {
            printk("Failed to send MQTT PUBREL: %d", err);
        }
        break;

    case MQTT_EVT_PUBCOMP:
        if (evt->result != 0) {
            printk("MQTT PUBCOMP error %d\n", evt->result);
            break;
        }
        printk("PUBCOMP packet id: %u", evt->param.pubcomp.message_id);
        break;

    case MQTT_EVT_SUBACK:
        if (evt->result != 0) {
            printk("MQTT SUBACK error %d\n", evt->result);
            break;
        }
        printk("SUBACK packet id: %u", evt->param.suback.message_id);
        break;

    case MQTT_EVT_PINGRESP:
        printk("PINGRESP packet\n");
        break;

    default:
        printk("Unhandled MQTT event type: %d", evt->type);
        break;
    }
}


static int broker_init(void) {
    int err;
    struct addrinfo *result;
    struct addrinfo *addr;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
    if (err) {
        printk("getaddrinfo failed: %d\n", err);
        return -ECHILD;
    }

    addr = result;

    /* Look for address of the broker */
    while (addr != NULL) {
        /* IPv4 address */
        if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
            struct sockaddr_in *broker4 = ((struct sockaddr_in *)&broker);
            char ipv4_addr[NET_IPV4_ADDR_LEN];

            broker4->sin_addr.s_addr = ((struct sockaddr_in *)addr->ai_addr)->sin_addr.s_addr;
            broker4->sin_family = AF_INET;
            broker4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);

            inet_ntop(AF_INET, &broker4->sin_addr.s_addr, ipv4_addr, sizeof(ipv4_addr));
            printk("IPv4 Address found %s\n", ipv4_addr);

            break;
        } else {
            printk("ai_addrlen = %u should be %u or %u\n", 
                (unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
        }

        addr = addr->ai_next;
    }

    /* Free the address */
    freeaddrinfo(result);

    return err;
}


void gen_jwt() {
    struct jwt_builder jwt;
    jwt_init_builder(&jwt, jwt_buf, sizeof(jwt_buf));

    int64_t time;
    date_time_now(&time);
    int32_t now = time / 1000;

    jwt_add_payload(&jwt, now+3600, now, "wearebrews");

    jwt_sign(&jwt, private_der, private_der_len);
    size_t len = jwt_payload_len(&jwt);

    jwt_buf[len] = '\0';
}


static int client_init(struct mqtt_client *client) {

    int err;
    
    mqtt_client_init(client);

    err = broker_init();
    if (err) {
		printk("Failed to initialize broker connection");
		return err;
	}

    gen_jwt();

    static struct mqtt_utf8 username = MQTT_UTF8_LITERAL("stray");
    static struct mqtt_utf8 password;

    /* MQTT client config */
    client->broker = &broker;
    client->evt_cb = mqtt_evt_handler;
    client->client_id.utf8 = CONFIG_MQTT_CLIENT_ID;
    client->client_id.size = sizeof(CONFIG_MQTT_CLIENT_ID) - 1;
    client->password = &password;
    client->password->size = strlen(jwt_buf);
    client->password->utf8 = jwt_buf;
    client->user_name = &username;
    client->protocol_version = MQTT_VERSION_3_1_1;

    /* MQTT buffers config */
    client->rx_buf = rx_buffer;
    client->rx_buf_size = sizeof(rx_buffer);
    client->tx_buf = tx_buffer;
    client->tx_buf_size = sizeof(tx_buffer);

    /* MQTT transport config */
    struct mqtt_sec_config *tls_cfg = &(client->transport).tls.config;
    static sec_tag_t sec_tag_list[] = { PRIMARY_SEC_TAG, BACKUP_SEC_TAG };

    client->transport.type = MQTT_TRANSPORT_SECURE;

    tls_cfg->peer_verify = CONFIG_MQTT_TLS_PEER_VERIFY;
    tls_cfg->cipher_count = 0;
    tls_cfg->cipher_list = NULL;
    tls_cfg->sec_tag_count = ARRAY_SIZE(sec_tag_list);
    tls_cfg->sec_tag_list = sec_tag_list;
    tls_cfg->hostname = CONFIG_MQTT_BROKER_HOSTNAME;
    tls_cfg->session_cache = IS_ENABLED(CONFIG_MQTT_TLS_SESSION_CACHING) ?
                        TLS_SESSION_CACHE_ENABLED :
                        TLS_SESSION_CACHE_DISABLED;

    return err;
}


static int fds_init(struct mqtt_client *client) {
    if (client->transport.type == MQTT_TRANSPORT_SECURE) {
        fds.fd = client->transport.tls.sock;
    } else {
        return -ENOTSUP;
    }

    fds.events = POLLIN;

    return 0;
}


static int modem_configure(void) {
    printk("Disabling PSM and eDRX\n");
    lte_lc_psm_req(false);
    lte_lc_edrx_req(false);

    int err;
    printk("LTE Link Connecting...\n");
    err = lte_lc_init_and_connect();
    if (err) {
        printk("Failed to establish LTE connection: %d\n", err);
        return err;
    }
    printk("LTE Link Connected/n");

    return 0;
}

void date_time_evt_handler(const struct date_time_evt *evt) {
    k_sem_give(&time_sem);
}



void mqtt_work_handler(struct k_work *work) {
    char coordinates[10 * sizeof(gps_msg_type)];
    gps_msg_type gps_msg;
    k_msgq_get(&gps_msgq, &gps_msg, K_FOREVER);

    /* Format GPS coordinates to "(latitude;longitude)" */
    gps_msg.latitude = 63.09124712;
    gps_msg.longitude = 15.12419553;
    sprintf(coordinates, "%f;%f", gps_msg.latitude, gps_msg.longitude);
    printk("Coordinates: %s\n", coordinates);

//     int err;
//     uint32_t connect_attempt = 0;
//     printk("Starting MQTT connection\n");

//     err = certificates_provision();
//     if (err != 0) {
//         printk("Failed to provision certificates\n");
//         return;
//     }

//     do {
//         err = modem_configure();
//         if (err) {
//             printk("Retrying in %d seconds\n", CONFIG_LTE_CONNECT_RETRY_DELAY_S);
//             k_sleep(K_SECONDS(CONFIG_LTE_CONNECT_RETRY_DELAY_S));
//         }
//     } while(err);

//     /* Sync time */
//     date_time_update_async(date_time_evt_handler);
//     k_sem_take(&time_sem, K_FOREVER);

//     err = client_init(&client_ctx);
//     if (err != 0) {
//         printk("client_init: %d\n", err);
//         return;
//     }

// do_connect:
//     if (connect_attempt++ > 0) {
//         printk("Reconnecting in %d seconds...\n", CONFIG_MQTT_RECONNECT_DELAY_S);
//         k_sleep(K_SECONDS(CONFIG_MQTT_RECONNECT_DELAY_S));
//     }
//     err = mqtt_connect(&client_ctx);
//     if (err != 0) {
//         printk("mqtt_connect %d\n", err);
//         goto do_connect;
//     }

//     err = fds_init(&client_ctx);
//     if (err != 0) {
// 		printk("fds_init: %d\n", err);
// 		return;
// 	}
    

//     while(1) {
//         err = poll(&fds, 1, mqtt_keepalive_time_left(&client_ctx));
// 		if (err < 0) {
// 			printk("poll: %d\n", err);
// 			break;
// 		}

// 		err = mqtt_live(&client_ctx);
// 		if ((err != 0) && (err != -EAGAIN)) {
// 			printk("ERROR: mqtt_live: %d\n", err);
// 			break;
// 		}

// 		if ((fds.revents & POLLIN) == POLLIN) {
// 			err = mqtt_input(&client_ctx);
// 			if (err != 0) {
// 				printk("mqtt_input: %d\n", err);
// 				break;
// 			}
// 		}

// 		if ((fds.revents & POLLERR) == POLLERR) {
// 			printk("POLLERR\n");
// 			break;
// 		}

// 		if ((fds.revents & POLLNVAL) == POLLNVAL) {
// 			printk("POLLNVAL\n");
// 			break;
// 		}

//     }

//     printk("Disconnecting MQTT client...\n");

//     err = mqtt_disconnect(&client_ctx);
//     if (err) {
//         printk("Could not disconnect MQTT client: %d\n", err);
//     }
//     goto do_connect;
}


K_WORK_DEFINE(mqtt_work, mqtt_work_handler);

void mqtt_request_weather() {
    k_work_submit(&mqtt_work);
}

