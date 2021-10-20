
#include <zephyr.h>
#include <stdio.h>
#include <drivers/uart.h>
#include <string.h>
#include <random/rand32.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <modem/at_cmd.h>
#include <modem/lte_lc.h>
#include <logging/log.h>
#if defined(CONFIG_MODEM_KEY_MGMT)
#include <modem/modem_key_mgmt.h>
#endif

/*
#define APP_CONNECT_TIMEOUT_MS	2000
#define APP_SLEEP_MSECS		    500
#define APP_CONNECT_TRIES       10
#deinfe APP_MAX_ITERATIONS      500
*/

// Buffers for MQTT client
static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];

// MQTT client context
static struct mqtt_client client_ctx;

// MQTT Broker address info
static struct sockaddr_storage broker;

// File descrciptor
static struct pollfd fds;


//static bool connected;




static int publish(struct mqtt_client *client, enum mqtt_qos qos, uint8_t *data, size_t len) {
    struct mqtt_publish_param param;

    param.message.topic.qos = qos;
    param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
    param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
    param.message.payload.data = data;
    param.message.payload.len = len;
    param.message_id = sys_rand32_get();
    param.dup_flag = 0;
    param.retain_flag = 0;

    return mqtt_publish(client, &param);
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
        //connected = true;
        printk("MQTT client connected!\n");
        subscribe();
        break;
        
    case MQTT_EVT_DISCONNECT:
        printk("MQTT client disconnected %d\n", evt->result);
        //connected = false;
        //clear_fds();
        break;

    case MQTT_EVT_PUBLISH: {
        const struct mqtt_publish_param *p = &evt->param.publish;
        printk("MQTT PUBLISH result: %d\n", evt->result);
        err = publish_get_payload(client, p->message.payload.len);

        if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
            const struct mqtt_puback_param ack = {
                .message_id = p->message_id
            };

            // Send acknowledgement
            mqtt_publish_qos1_ack(&client_ctx, &ack);
        }

        if (err >= 0) {
            printk("Data received: %hhn", payload_buf);
            // Echo back received data
            publish(&client_ctx, MQTT_QOS_1_AT_LEAST_ONCE);
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

    //case MQTT_EVT_PUBREL:

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

    //case MQTT_EVT_UNSUBACK:

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

    // Look for address of the broker
    while (addr != NULL) {
        // IPv4 address
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

    // Free the address
    freeaddrinfo(result);

    return err;
}



static int client_init(struct mqtt_client *client) {

    int err;
    
    mqtt_client_init(client);

    err = broker_init();
    if (err) {
		printk("Failed to initialize broker connection");
		return err;
	}

    // MQTT client config
    client->broker = &broker;
    client->evt_cb = mqtt_evt_handler;
    client->client_id.utf8 = (uint8_t *)"zephyr_mqtt_client";
    client->client_id.size = sizeof("zephyr_mqtt_client") - 1;
    client->password = NULL;
    client->user_name = NULL;
    client->protocol_version = MQTT_VERSION_3_1_1;
    client->transport.type = MQTT_TRANSPORT_NON_SECURE;

    // MQTT buffers config
    client->rx_buf = rx_buffer;
    client->rx_buf_size = sizeof(rx_buffer);
    client->tx_buf = tx_buffer;
    client->tx_buf_size = sizeof(tx_buffer);

    return err;
}

static int fds_init(struct mqtt_client *client) {
    if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
        fds.fd = client->transport.tcp.sock;
    } else {
        return -ENOTSUP;
    }

    fds.events = POLLIN;

    return 0;
}

/*
static int try_to_connect(struct mqtt_client *client) {
    int rc, i = 0;

    while ((i++ < APP_CONNECT_TRIES) && !connected) {
        client_init(client);

        rc = mqtt_connect(client);
        if (rc != 0) {
            printk("mqtt_connect %d\n", rc);
            k_sleep(K_MSEC(APP_SLEEP_MSECS));
            continue;
        }

        // ???
        fds[0].fd = client_ctx.transport.tcp.sock;
        fds[0].events = ZSOCK_POLLIN;

        if (wait(APP_CONNECT_TIMEOUT_MS)) {
            mqtt_input(client);
        }

        if (!connected) {
            mqtt_abort(client);
        }
    }

    if (connected) {
        return 0;
    }

    return -EINVAL;
}


static int process_mqtt_and_sleep(struct mqtt_client *client, int timeout) {
    int64_t remaining = timeout;
    int64_t start_time = k_uptime_get();
    int rc;

    while ((remaining > 0) && connected) {
        if (wait(remaining)) {
            rc = mqtt_input(client);
            if (rc != 0) {
                printk("mqtt_input %d\n", rc);
                return rc;
            }
        }

        rc = mqtt_live(client);
        if ((rc != 0) && (rc != -EAGAIN)) {
            printk("mqtt_live %d\n", rc);
            return rc;
        } else if (rc == 0) {
            rc = mqtt_input(client);
            if (rc != 0) {
                printk("mqtt_input %d\n", rc);
                return rc;
            }
        }

        remaining = timeout + start_time - k_uptime_get();
    }

    return 0;
}


static int publisher(void) {
    int i, rc, r = 0;

    printk("Attempting to connect: ");
    rc = try_to_connect(&client_ctx);
    printk("try_to_connect %d\n", rc);
    if (rc != 0) {
        return 1;
    }

    i = 0;
    while ((i++ < APP_MAX_ITERATIONS) && connected) {
        r = -1;

        rc = mqtt_ping(&client_ctx);
        printk("mqtt_ping %d\n", rc);
        if (rc != 0) {
            break;
        }

        rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
        if (rc != 0) {
            break;
        }

        rc = publish(&client_ctx, MQQT_QOS_1_AT_LEAST_ONCE);
        printk("mqtt_publish %d\n", rc);
        if (rc != 0) {
            break;
        }

        rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
        if (rc != 0) {
            break;
        }

        r = 0;    
    }

    rc = mqtt_disconnect(&client_ctx);
    printk("mqtt_disconnect %d\n", rc);

    return r;
}
*/

// void main() {

//     int r, i = 0;

//     while (!APP_MAX_ITERATIONS || (i++ < APP_MAX_ITERATIONS)) {
//         r = publisher();

//         if (!APP_MAX_ITERATIONS) {
//             k_sleep(K_MSEC(5000));
//         }
//     }

//     return r;
// }


void main(void) {
    int err;
    uint32_t connect_attempt = 0;
    printk("Starting MQTT connection\n");

    err = client_init(&client_ctx);
    if (err != 0) {
        printk("client_init: %d\n", err);
        return;
    }

do_connect:
    if (connect_attempt++ > 0) {
        printk("Reconnecting in %d seconds...\n", CONFIG_MQTT_RECONNECT_DELAY_S);
        k_sleep(K_SECONDS(CONFIG_MQTT_RECONNECT_DELAY_S));
    }
    err = mqtt_connect(&client_ctx);
    if (err != 0) {
        printk("mqtt_connect %d\n", err);
        goto do_connect;
    }

    err = fds_init(&client_ctx);
    if (err != 0) {
		printk("fds_init: %d\n", err);
		return;
	}
    

    while(1) {
        err = poll(&fds, 1, mqtt_keepalive_time_left(&client_ctx));
		if (err < 0) {
			printk("poll: %d\n", err);
			break;
		}

		err = mqtt_live(&client_ctx);
		if ((err != 0) && (err != -EAGAIN)) {
			printk("ERROR: mqtt_live: %d\n", err);
			break;
		}

		if ((fds.revents & POLLIN) == POLLIN) {
			err = mqtt_input(&client_ctx);
			if (err != 0) {
				printk("mqtt_input: %d\n", err);
				break;
			}
		}

		if ((fds.revents & POLLERR) == POLLERR) {
			printk("POLLERR\n");
			break;
		}

		if ((fds.revents & POLLNVAL) == POLLNVAL) {
			printk("POLLNVAL\n");
			break;
		}
    }

    printk("Disconnecting MQTT client...\n");

    err = mqtt_disconnect(&client_ctx);
    if (err) {
        printk("Could not disconnect MQTT client: %d\n", err);
    }
    goto do_connect;
}

