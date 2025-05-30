// async_consume_v5.cpp
//
// This is a Paho MQTT C++ client, sample application.
//
// This application is an MQTT consumer/subscriber using the C++
// asynchronous client interface, employing the  to receive messages
// and status updates.
//
// The sample demonstrates:
//  - Connecting to an MQTT v5 server/broker.
//  - Subscribing to a topic
//  - Receiving messages through the consuming (queuing) API
//

/*******************************************************************************
 * Copyright (c) 2013-2024 Frank Pagliughi <fpagliughi@mindspring.com>
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Frank Pagliughi - initial implementation and documentation
 *******************************************************************************/

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include "mqtt/async_client.h"

using namespace std;

const string DFLT_SERVER_URI{"mqtt://localhost:1883"};
const string CLIENT_ID{"PahoCppAsyncConsumeV5"};

// const string TOPIC{"hello"};
const string TOPIC{"#"};
const int QOS = 1;

/////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    auto serverURI = (argc > 1) ? string{argv[1]} : DFLT_SERVER_URI;

    auto cli = std::make_shared<mqtt::async_client>(serverURI, CLIENT_ID);

    auto connOpts = mqtt::connect_options_builder::v5()
                        .clean_start(false)
                        .properties({{mqtt::property::SESSION_EXPIRY_INTERVAL, 604800}})
                        .finalize();

    try {
        // Start consumer before connecting to make sure to not miss messages

        cli->start_consuming();

        // Connect to the server

        cout << "Connecting to the MQTT server..." << flush;
        auto tok = cli->connect(connOpts);

        // Getting the connect response will block waiting for the
        // connection to complete.
        auto rsp = tok->get_connect_response();

        // Make sure we were granted a v5 connection.
        if (rsp.get_mqtt_version() < MQTTVERSION_5) {
            cout << "\n  Did not get an MQTT v5 connection." << flush;
            exit(1);
        }

        // If there is no session present, then we need to subscribe, but if
        // there is a session, then the server remembers us and our
        // subscriptions.
        if (!rsp.is_session_present()) {
            cout << "\n  Session not present on broker. Subscribing..." << flush;
            cli->subscribe(TOPIC, QOS)->wait();
        }

        cout << "\n  OK" << endl;

        // We'll signal the consumer to exit from another thread.
        // (just to show that we can)
        thread([cli] {
            this_thread::sleep_for(60s);
            cout << "\nClosing the consumer." << endl;
            cli->stop_consuming();
        }).detach();

        // Consume messages
        //
        // This just exits if the consumer is closed or the client is
        // disconnected. (See some other examples for auto or manual
        // reconnect)

        cout << "\nWaiting for messages on topic: '" << TOPIC << "'" << endl;

        try {
            while (true) {
                auto evt = cli->consume_event();

                if (const auto* p = evt.get_message_if()) {
                    auto& msg = *p;
                    if (!msg)
                        continue;

                    cout << msg->get_topic() << ": " << msg->to_string();

                    const auto& props = msg->get_properties();
                    if (size_t n = props.size(); n != 0) {
                        cout << "\n  [";
                        for (size_t i = 0; i < n - 1; ++i) cout << props[i] << ", ";
                        cout << props[n - 1] << "]";
                    }
                    cout << endl;
                }
                else if (evt.is_connected()) {
                    cout << "\n*** Connected ***" << endl;
                }
                else if (evt.is_connection_lost()) {
                    cout << "*** Connection Lost ***" << endl;
                    break;
                }
                else if (const auto* p = evt.get_disconnected_if()) {
                    cout << "*** Disconnected. Reason [0x" << hex << int{p->reasonCode}
                         << "]: " << p->reasonCode << " ***" << endl;
                    break;
                }
            }
        }
        catch (mqtt::queue_closed&) {
        }

        // If we're here, the client was almost certainly disconnected.
        // But we check, just to make sure.

        if (cli->is_connected()) {
            cout << "\nShutting down and disconnecting from the MQTT server..." << flush;
            cli->disconnect()->wait();
            cout << "OK" << endl;
        }
    }
    catch (const mqtt::exception& exc) {
        cerr << "\n  " << exc << endl;
        return 1;
    }

    return 0;
}
