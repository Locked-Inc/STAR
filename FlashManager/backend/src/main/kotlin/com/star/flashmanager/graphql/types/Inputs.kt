package com.star.flashmanager.graphql.types

import com.star.flashmanager.domain.enums.Component
import com.star.flashmanager.domain.enums.DeviceCapability
import com.star.flashmanager.domain.enums.Platform

data class RegisterInput(
    val email: String,
    val password: String
)

data class LoginInput(
    val email: String,
    val password: String
)

data class RegisterDeviceInput(
    val name: String,
    val platform: Platform,
    val capabilities: List<DeviceCapability>
)

data class TriggerBuildInput(
    val component: Component,
    val branch: String?
)

data class CreateFlashJobInput(
    val buildJobId: String,
    val deviceId: String,
    val targetDevice: String,
    val priority: Int?
)
