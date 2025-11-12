package com.star.flashmanager.services

import com.star.flashmanager.domain.entities.Device
import com.star.flashmanager.domain.enums.DeviceCapability
import com.star.flashmanager.domain.enums.DeviceStatus
import com.star.flashmanager.domain.enums.Platform
import com.star.flashmanager.exception.ResourceNotFoundException
import com.star.flashmanager.repositories.DeviceRepository
import com.star.flashmanager.security.JwtTokenProvider
import org.springframework.security.crypto.password.PasswordEncoder
import org.springframework.stereotype.Service
import org.springframework.transaction.annotation.Transactional
import java.time.OffsetDateTime
import java.util.UUID

@Service
@Transactional
class DeviceService(
    private val deviceRepository: DeviceRepository,
    private val passwordEncoder: PasswordEncoder,
    private val jwtTokenProvider: JwtTokenProvider
) {

    fun getDeviceById(id: String): Device {
        return deviceRepository.findById(id)
            .orElseThrow { ResourceNotFoundException("Device not found with id: $id") }
    }

    fun getAllDevices(): List<Device> {
        return deviceRepository.findAll()
    }

    fun getDevicesByUserId(userId: String): List<Device> {
        return deviceRepository.findByUserId(userId)
    }

    fun registerDevice(
        name: String,
        userId: String,
        platform: Platform,
        capabilities: Set<DeviceCapability>
    ): DeviceRegistrationResult {
        val apiKey = UUID.randomUUID().toString()
        val device = Device(
            id = UUID.randomUUID().toString(),
            name = name,
            apiKeyHash = passwordEncoder.encode(apiKey),
            userId = userId,
            status = DeviceStatus.OFFLINE,
            platform = platform,
            capabilities = capabilities.toMutableSet()
        )

        val savedDevice = deviceRepository.save(device)

        return DeviceRegistrationResult(
            device = savedDevice,
            apiKey = apiKey
        )
    }

    fun updateDeviceStatus(deviceId: String, status: DeviceStatus): Device {
        val device = getDeviceById(deviceId)
        device.status = status
        device.lastSeen = OffsetDateTime.now()
        return deviceRepository.save(device)
    }

    fun updateDeviceHeartbeat(deviceId: String): Device {
        val device = getDeviceById(deviceId)
        device.lastSeen = OffsetDateTime.now()
        device.status = DeviceStatus.ONLINE
        return deviceRepository.save(device)
    }

    fun deleteDevice(deviceId: String): Boolean {
        if (!deviceRepository.existsById(deviceId)) {
            throw ResourceNotFoundException("Device not found with id: $deviceId")
        }
        deviceRepository.deleteById(deviceId)
        return true
    }

    fun deviceLogin(apiKey: String): DeviceLoginResult {
        // Find device by checking apiKey hash
        val devices = deviceRepository.findAll()
        val device = devices.firstOrNull { passwordEncoder.matches(apiKey, it.apiKeyHash) }
            ?: throw ResourceNotFoundException("Invalid API key")

        val token = jwtTokenProvider.generateDeviceToken(device.id)

        // Update device status
        device.status = DeviceStatus.ONLINE
        device.lastSeen = OffsetDateTime.now()
        deviceRepository.save(device)

        return DeviceLoginResult(
            token = token,
            device = device
        )
    }

    data class DeviceRegistrationResult(
        val device: Device,
        val apiKey: String
    )

    data class DeviceLoginResult(
        val token: String,
        val device: Device
    )
}
