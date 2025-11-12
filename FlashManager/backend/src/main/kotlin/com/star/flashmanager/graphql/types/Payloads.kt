package com.star.flashmanager.graphql.types

import com.star.flashmanager.domain.entities.Device
import com.star.flashmanager.domain.entities.User

data class AuthPayload(
    val accessToken: String,
    val refreshToken: String,
    val user: User? = null,
    val device: Device? = null
)

data class DeviceRegistration(
    val device: Device,
    val apiKey: String
)
