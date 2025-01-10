# Arduino_rfidtracker_SMS
Combination of an RFID reader and a GSM module connected to an Arduino board. It send RFID tag details via SMS.

This project involves the combination of an RFID reader and a GSM module connected to an Arduino board. The system detects the proximity of one or more RFID chips registered in advance and each associated with a different athlete who is competing in a sporting discipline that involves completing a predetermined circuit one or more times, for example a triathlon.
The main objective is to complement the athlete tracking systems that the sports event's organizing body usually has in order to ensure that the information is richer, more personalized and in real time.
The system will be placed at an intermediate point of the circuit where the triathlete must pass repeatedly. At each step of the triathlete through that turnaround point, the RFID reader will detect the RFID chip (beacon) carried by the athlete and will send an SMS to the previously configured phone numbers of the followers. The text of the messages includes sports statistics for each lap, such as the time taken, competition pace, speed, number of laps, etc.
