package com.projectstar.serverbackend

import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.runApplication

@SpringBootApplication
class ServerBackendApplication

fun main(args: Array<String>) {
	@Suppress("SpreadOperator")
	runApplication<ServerBackendApplication>(*args)
}