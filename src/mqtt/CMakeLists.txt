# MQTT 模块:MqttClient(header-only),链接 libmosquitto
add_library(mqtt INTERFACE)
target_include_directories(mqtt INTERFACE .)
find_library(MOSQUITTO_LIB mosquitto REQUIRED)
find_path(MOSQUITTO_INC mosquitto.h REQUIRED)
target_include_directories(mqtt INTERFACE ${MOSQUITTO_INC})
target_link_libraries(mqtt INTERFACE ${MOSQUITTO_LIB})
