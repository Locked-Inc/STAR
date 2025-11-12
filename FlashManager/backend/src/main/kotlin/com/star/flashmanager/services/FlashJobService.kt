package com.star.flashmanager.services

import com.star.flashmanager.domain.entities.FlashJob
import com.star.flashmanager.domain.enums.JobStatus
import com.star.flashmanager.exception.ResourceNotFoundException
import com.star.flashmanager.repositories.BuildJobRepository
import com.star.flashmanager.repositories.DeviceRepository
import com.star.flashmanager.repositories.FlashJobRepository
import org.springframework.stereotype.Service
import org.springframework.transaction.annotation.Transactional
import java.util.UUID

@Service
@Transactional
class FlashJobService(
    private val flashJobRepository: FlashJobRepository,
    private val buildJobRepository: BuildJobRepository,
    private val deviceRepository: DeviceRepository
) {

    fun createFlashJob(
        buildJobId: String,
        deviceId: String,
        targetDevice: String,
        priority: Int?
    ): FlashJob {
        // Validate build job exists and is completed
        val buildJob = buildJobRepository.findById(buildJobId)
            .orElseThrow { ResourceNotFoundException("Build job not found with id: $buildJobId") }

        check(buildJob.status == JobStatus.COMPLETED) {
            "Build job must be completed before creating flash job"
        }

        check(buildJob.binaryPath != null) {
            "Build job has no binary path"
        }

        // Validate device exists
        check(deviceRepository.existsById(deviceId)) {
            "Device not found with id: $deviceId"
        }

        val flashJob = FlashJob(
            id = UUID.randomUUID().toString(),
            buildJobId = buildJobId,
            deviceId = deviceId,
            targetDevice = targetDevice,
            status = JobStatus.PENDING,
            priority = priority ?: 0
        )

        return flashJobRepository.save(flashJob)
    }

    fun getFlashJobById(id: String): FlashJob {
        return flashJobRepository.findById(id)
            .orElseThrow { ResourceNotFoundException("Flash job not found with id: $id") }
    }

    fun claimFlashJob(id: String, deviceId: String): FlashJob {
        val flashJob = getFlashJobById(id)

        check(flashJob.deviceId == deviceId) {
            "Flash job does not belong to this device"
        }

        check(flashJob.status == JobStatus.PENDING) {
            "Flash job is not in PENDING state"
        }

        flashJob.status = JobStatus.RUNNING
        return flashJobRepository.save(flashJob)
    }

    fun updateFlashStatus(id: String, status: JobStatus): FlashJob {
        val flashJob = getFlashJobById(id)
        flashJob.status = status
        return flashJobRepository.save(flashJob)
    }

    fun completeFlashJob(id: String, success: Boolean): FlashJob {
        val flashJob = getFlashJobById(id)
        flashJob.status = if (success) JobStatus.COMPLETED else JobStatus.FAILED
        return flashJobRepository.save(flashJob)
    }

    fun cancelFlashJob(id: String): Boolean {
        val flashJob = getFlashJobById(id)

        check(flashJob.status != JobStatus.COMPLETED && flashJob.status != JobStatus.FAILED) {
            "Cannot cancel flash job in ${flashJob.status} state"
        }

        flashJob.status = JobStatus.FAILED
        flashJobRepository.save(flashJob)

        return true
    }
}
