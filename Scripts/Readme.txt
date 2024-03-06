the ESP32 script
 - Establishes a connection to the internet portal
 - Every 30 seconds it polls it
 - Reports over MQTT the battery %, the Grid Usage and the House Usage
 - The Grid and House usage is not accurate, use the other downloader to get a more accurate estimate
 - It also allows to set the discharge time 1 via MQTT. Example payload of 06:00 would set the end discharge time to 6AM
 - the SetSettings payload, all of those variables are needed for it to be valid, it will overwrite most of your Settings
 - The credentials at the top are important. set them all. Authsignature i sniffed one from a login page, just use that for everything

energyDownloader_v001
 - Connects to the internet portal
 - downloads all json files between two dates

 analyser
 - points to a folder
 - runs through all the json files and determines how much was used on each day based on the TOU pricing
