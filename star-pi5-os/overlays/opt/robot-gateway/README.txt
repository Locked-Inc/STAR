STAR Robot Gateway Deployment
==============================

This directory is where the robot-gateway JAR file should be placed.

To deploy your robot-gateway application:

1. Build your Spring Boot JAR file on your development machine:
   cd /path/to/STAR/robot-gateway
   ./gradlew bootJar
   # or: ./mvnw package

2. Copy the JAR file to the robot:
   scp target/robot-gateway.jar root@star-robot.local:/opt/robot-gateway/

3. Enable and start the service:
   systemctl enable robot-gateway
   systemctl start robot-gateway

4. Check service status:
   systemctl status robot-gateway
   journalctl -u robot-gateway -f

5. To restart after updates:
   systemctl restart robot-gateway

Note: The service is configured to auto-start on boot and restart on failure.
If you need to change the JAR filename, edit /etc/systemd/system/robot-gateway.service
