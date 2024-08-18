# Smart_bulb

Explaination Video:
https://youtu.be/2seQ4D1FWOw


![flight](https://github.com/user-attachments/assets/ee3ed8df-472c-4400-b3dd-8215eaa3916f)

The project involves the development and scaling of a smart RGB LED device, designed for integration with a custom application, Apple HomeKit, and Amazon Alexa. Initially, the device pairs with a custom mobile app to facilitate Wi-Fi configuration through user authentication. This is achieved using Firebase for secure user registration and device association. Post configuration, the device transitions from setup to operational mode, where it automatically connects to the specified Wi-Fi network. This setup allows for subsequent pairing with Apple HomeKit or Amazon Alexa, expanding control interfaces. Communication between the custom app and the smart device (built on ESP32) is managed via an MQTT broker, enabling real-time data exchange and control commands. This MQTT-based interaction ensures efficient, scalable, and responsive device operations, crucial for IoT environments. The use of Firebase not only aids in user authentication but also supports device sharing among multiple users, enhancing the device’s functionality and user experience. This comprehensive integration approach facilitates a robust and user-friendly smart home ecosystem.
