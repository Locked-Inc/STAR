package com.projectstar.robotgateway

import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.runApplication

@SpringBootApplication
class RobotGatewayApplication

fun main(args: Array<String>) {
	@Suppress("SpreadOperator")
	runApplication<RobotGatewayApplication>(*args)
}
