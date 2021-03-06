-- Global variables and parameters.
nodeID = 4                -- a node identifier for this device
sleepTime = 12000         -- duration of the energy saving mode

-- Variables for connection to the Brocker
tgtHost = "192.168.43.203"    -- target host (broker)
tgtPort = 1883              -- target port (broker listening on)
mqttUserID = "mosquitto"    -- account to use to log into the broker
mqttPass = "password"       -- broker account password
mqttTimeOut = 120           -- connection timeout       
messageReceive = 1          -- data transmission interval in seconds 

-- Topic variables
configTopic = "/system_name/" .. nodeID .. "/" .. "config" 
configAckTopic = "/system_name/" .. nodeID .. "/config_ack"
discoverTopic = "/system_name/" .. "discover"

-- Op_Mode variables
opMode = 2
modeIDLE = "IDLE"
modeSensor = "Sensor"
modeRelay = "Relay"
modeSensorAndRelay = "Sensor and Relay"

--Op_mode parameters
IDLE = 1
Senosr = 2
Relay = 3
SensorAndRelay = 4 

SDA_PIN = 2 -- sda pin, GPIO4
SCL_PIN = 1 -- scl pin, GPIO5

Temp = 0
relayValue = 24
    
-- Reconnect to MQTT when we receive an "offline" message.
function reconn()
    print("Disconnected, reconnecting....")
    conn()
end  
   
-- Establish a connection to the MQTT broker with the configured parameters.    
function conn()
    print("Making connection to MQTT broker")
    
    client:connect( "192.168.1.18" , 1883, 0, 
        function(client) print("Connected") 
      end,
      
      function(client, reason)
        print("Failed readon: " .. reason)
    end)
    
end

function mqttSub()
    client:subscribe(configTopic, 0,
        function(conn) print("Subscribe for " .. configTopic .. " successful")
        main()
        end)
end


-- Function pubEvent() publishes the data to the defined queue.   
function pubEvent(message, topic)
    -- Print a status message
    print("Publishing to " .. topic .. ": " .. message)
    mqttBroker:publish(topic, message, 0, 0)  -- publish
end

function getData()

    si7021 = require("si7021")
    si7021.init(SDA_PIN, SCL_PIN)   --  Setting the i2c pin of si7021
    si7021.read(OSS)
        
    Hum = si7021.getHumidity()      -- Get humidity from si7021
    Temp = si7021.getTemperature()  -- Get temperature from si7021
    
    Hum = Hum / 100 .. "." .. Hum % 100 
    Temp = Temp / 100 .. "." .. (Temp % 100)
    
    -- pressure in differents units
    print("Humidity: ".. Hum)
    
    -- temperature in degrees Celsius
    print("Temperature: ".. Temp)
    data = Temp .. "," .. Hum
    -- release module 
    si7021 = nil
    package.loaded["si7021"] = nil

    data = temp .. "," .. hum

    pubEvent(data, configAckTopic)
end

function sensorAndRelayMode()
    if Temp.tonumber(10) <= relayValue
    then
        --turn ON relay
    else
        -- turn OFF relay
    end
end

function wake()
    --conect to wrifi
    --conect to brocker 
    -- if mode = sensor or senro and relay -> get data

    --if opMode == Sensor or opMode == SensorAndRelay then
    getData()
    --end
end

function messageArrieved(client, topic, data)
    print("Delivered message on topic" .. topic .. ":" ) 

    if data ~= nil then
        print(data)
        messageReceive = 0
    end
       
    if string.find(data, modeIDLE) then
        opMode = IDLE
    elseif (string.find(data, modeSensor))
    then
        opMode = Sensor
        getData()
    elseif (string.find(data, modeRelay)) 
    then
        -- set op_Mode to Relay
        opMode = Relay
        -- get the value for the Relay
    elseif (string.find(data, modeSensorAndRelay)) 
    then
        -- set op_mode to Sensor and Relay
        opMode = SensorAndRelay
        getData()
    elseif (string.find(data, "relayValue"))
    then 
        relayValue = string.match(data, "%d+")
        sensroAndRelayMode()
    end    
end
 
-- makeConn() instantiates the MQTT control object, sets up callbacks,
-- This is the "main" function in this library. This should be called 
-- from init.lua (which runs on the ESP8266 at boot), but only after
-- it's been vigorously debugged. 
  
function makeConn()
    -- Instantiate a global MQTT client object
    print("Instantiating mqttBroker")
    client = mqtt.Client(nodeID, mqttTimeOut)
     
    -- Set up the event callbacks
    print("Setting up callbacks")
    client:on("connect", function(client) print ("connected") end)
    client:on("offline", reconn)
    
    client:on("message", messageArrieved)
    
    -- Connect to the Broker
    conn()
end

function main()    
    --while (messageReceive > 0)
    --do
        pubEvent(nodeID, discoverTopic)
        --pubEvent("24,38%,2018/11/24 15:45:35", "/system_name/1/data")
        messageReceive = 0
    --end
end