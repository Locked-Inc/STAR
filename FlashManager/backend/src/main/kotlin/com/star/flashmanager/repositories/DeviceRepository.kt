package com.star.flashmanager.repositories

import com.star.flashmanager.domain.entities.Device
import com.star.flashmanager.domain.enums.DeviceStatus
import org.springframework.data.jpa.repository.JpaRepository
import org.springframework.stereotype.Repository

@Repository
interface DeviceRepository : JpaRepository<Device, String> {
    fun findByUserId(userId: String): List<Device>
    fun findByStatus(status: DeviceStatus): List<Device>
    fun findByUserIdAndStatus(userId: String, status: DeviceStatus): List<Device>
}
