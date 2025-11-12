package com.star.flashmanager.graphql.resolvers

import com.star.flashmanager.domain.entities.BuildJob
import com.star.flashmanager.domain.entities.BuildLog
import com.star.flashmanager.domain.entities.Device
import com.star.flashmanager.domain.entities.FlashHistory
import com.star.flashmanager.domain.entities.FlashJob
import com.star.flashmanager.domain.entities.User
import com.star.flashmanager.domain.enums.Component
import com.star.flashmanager.domain.enums.JobStatus
import com.star.flashmanager.repositories.BuildJobRepository
import com.star.flashmanager.repositories.BuildLogRepository
import com.star.flashmanager.repositories.DeviceRepository
import com.star.flashmanager.repositories.FlashHistoryRepository
import com.star.flashmanager.repositories.FlashJobRepository
import com.star.flashmanager.security.DevicePrincipal
import com.star.flashmanager.security.UserPrincipal
import com.star.flashmanager.services.DeviceService
import com.star.flashmanager.services.UserService
import org.springframework.data.domain.PageRequest
import org.springframework.graphql.data.method.annotation.Argument
import org.springframework.graphql.data.method.annotation.QueryMapping
import org.springframework.security.access.prepost.PreAuthorize
import org.springframework.security.core.context.SecurityContextHolder
import org.springframework.stereotype.Controller

@Controller
@Suppress("TooManyFunctions")
class QueryResolver(
    private val userService: UserService,
    private val deviceService: DeviceService,
    private val buildJobRepository: BuildJobRepository,
    private val buildLogRepository: BuildLogRepository,
    private val flashJobRepository: FlashJobRepository,
    private val flashHistoryRepository: FlashHistoryRepository
) {

    // ========== User Queries ==========

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun me(): User {
        val userId = extractUserId()
        return userService.getUserById(userId)
    }

    @QueryMapping
    @PreAuthorize("hasRole('ADMIN')")
    fun users(): List<User> {
        return userService.getAllUsers()
    }

    @QueryMapping
    @PreAuthorize("hasRole('ADMIN')")
    fun user(@Argument id: String): User? {
        return userService.getUserById(id)
    }

    // ========== Device Queries ==========

    @QueryMapping
    @PreAuthorize("hasRole('ADMIN')")
    fun devices(): List<Device> {
        return deviceService.getAllDevices()
    }

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun device(@Argument id: String): Device? {
        return deviceService.getDeviceById(id)
    }

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun myDevices(): List<Device> {
        val userId = extractUserId()
        return deviceService.getDevicesByUserId(userId)
    }

    // ========== Build Queries ==========

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun buildJobs(
        @Argument status: JobStatus?,
        @Argument component: Component?,
        @Argument limit: Int?
    ): List<BuildJob> {
        return buildJobRepository.findByFilters(status, component)
            .let { jobs -> limit?.let { jobs.take(it) } ?: jobs }
    }

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun buildJob(@Argument id: String): BuildJob? {
        return buildJobRepository.findById(id).orElse(null)
    }

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun buildLogs(
        @Argument buildJobId: String,
        @Argument limit: Int?,
        @Argument offset: Int?
    ): List<BuildLog> {
        val actualLimit = limit ?: DEFAULT_LOG_LIMIT
        val actualOffset = offset ?: 0

        return if (actualLimit > 0 && actualOffset >= 0) {
            buildLogRepository.findByBuildJobIdWithPagination(
                buildJobId,
                PageRequest.of(actualOffset / actualLimit, actualLimit)
            )
        } else {
            buildLogRepository.findByBuildJobIdOrderByTimestampAsc(buildJobId)
        }
    }

    // ========== Flash Queries ==========

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun flashJobs(
        @Argument status: JobStatus?,
        @Argument deviceId: String?,
        @Argument limit: Int?
    ): List<FlashJob> {
        val jobs = when {
            deviceId != null && status != null -> flashJobRepository.findByDeviceIdAndStatus(deviceId, status)
            deviceId != null -> flashJobRepository.findByDeviceId(deviceId)
            status != null -> flashJobRepository.findByStatus(status)
            else -> flashJobRepository.findAll()
        }

        return limit?.let { jobs.take(it) } ?: jobs
    }

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun flashJob(@Argument id: String): FlashJob? {
        return flashJobRepository.findById(id).orElse(null)
    }

    @QueryMapping
    @PreAuthorize("isAuthenticated()")
    fun flashHistory(@Argument limit: Int?): List<FlashHistory> {
        val actualLimit = limit ?: DEFAULT_HISTORY_LIMIT
        return flashHistoryRepository.findAll()
            .sortedByDescending { it.createdAt }
            .take(actualLimit)
    }

    // ========== Agent Queries ==========

    @QueryMapping
    @PreAuthorize("hasAuthority('SCOPE_DEVICE')")
    fun pollFlashJobs(): List<FlashJob> {
        val deviceId = extractDeviceId()
        return flashJobRepository.findPendingJobsForDevice(deviceId)
    }

    // ========== Helper Methods ==========

    private fun extractUserId(): String {
        val principal = SecurityContextHolder.getContext().authentication?.principal
        return when (principal) {
            is UserPrincipal -> principal.id
            else -> error("User not authenticated")
        }
    }

    private fun extractDeviceId(): String {
        val principal = SecurityContextHolder.getContext().authentication?.principal
        return when (principal) {
            is DevicePrincipal -> principal.deviceId
            else -> error("Device not authenticated")
        }
    }

    companion object {
        private const val DEFAULT_LOG_LIMIT = 100
        private const val DEFAULT_HISTORY_LIMIT = 50
    }
}
