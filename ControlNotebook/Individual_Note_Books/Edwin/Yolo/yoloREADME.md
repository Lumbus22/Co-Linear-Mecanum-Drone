PERSON_PIN = 17 — BCM pin 17 by default, change it to whichever pin you have wired up                                   
Pin goes HIGH the moment a person is detected in a frame, LOW as soon as they leave                                     
atexit.register(GPIO.cleanup) ensures the pin is driven LOW and released cleanly when the script stops (Ctrl+C etc.)    
All other detected objects (car, dog, etc.) still get drawn on the stream but don't affect the pin
To switch modes, just change the one line near the top:                                                                                                             
  YOLO_ENABLED = True   # detection on
  YOLO_ENABLED = False  # raw stream, no overhead


Terminal Commands to control video stream:
sudo systemctl stop videostream      # stop now                               
sudo systemctl start videostream     # start now                              
sudo systemctl restart videostream   # restart (after editing the script)     
sudo systemctl disable videostream   # don't start on boot                    
sudo systemctl enable videostream    # start on boot (already set)            
systemctl status videostream         # check if running                       
journalctl -u videostream -f         # follow live logs
