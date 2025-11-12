package com.star.flashmanager.graphql.resolvers

import com.star.flashmanager.domain.entities.Device
import com.star.flashmanager.domain.entities.FlashJob
import com.star.flashmanager.domain.entities.BuildJob
import com.star.flashmanager.domain.enums.DeviceStatus
import com.star.flashmanager.domain.enums.FlashResult
import com.star.flashmanager.domain.enums.JobStatus
import com.star.flashmanager.graphql.types.AuthPayload
import com.star.flashmanager.graphql.types.CreateFlashJobInput
import com.star.flashmanager.graphql.types.DeviceRegistration
import com.star.flashmanager.graphql.types.LoginInput
import com.star.flashmanager.graphql.types.RegisterDeviceInput
import com.star.flashmanager.graphql.types.RegisterInput
import com.star.flashmanager.graphql.types.TriggerBuildInput
import com.star.flashmanager.security.DevicePrincipal
import com.star.flashmanager.security.UserPrincipal
import com.star.flashmanager.services.AuthService
import com.star.flashmanager.services.BuildJobService
import com.star.flashmanager.services.DeviceService
import com.star.flashmanager.services.FlashHistoryService
import com.star.flashmanager.services.FlashJobService
import com.star.flashmanager.services.UserService
import org.springframework.graphql.data.method.annotation.Argument
import org.springframework.graphql.data.method.annotation.MutationMapping
import org.springframework.security.access.prepost.PreAuthorize
import org.springframework.security.core.context.SecurityContextHolder
import org.springframework.stereotype.Controller

@Controller
@Suppress("TooManyFunctions")
class MutationResolver(
    private val authService: AuthService,
    private val deviceService: DeviceService,
    private val buildJobService: BuildJobService,
    private val flashJobService: FlashJobService,
    private val flashHistoryService: FlashHistoryService
) {

    // ========== Authentication ==========

    @MutationMapping
    fun register(@Argument input: RegisterInput): AuthPayload {
        val result = authService.register(input.email, input.password)
        return AuthPayload(
            accessToken = result.accessToken,
            refreshToken = result.refreshToken,
            user = result.user
        )
    }

    @MutationMapping
    fun login(@Argument input: LoginInput): AuthPayload {
        val result = authService.login(input.email, input.password)
        return AuthPayload(
            accessToken = result.accessToken,
            refreshToken = result.refreshToken,
            user = result.user
        )
    }

    @MutationMapping
    @PreAuthorize("isAuthenticated()")
    fun refreshToken(@Argument refreshToken: String): AuthPayload {
        val result = authService.refreshToken(refreshToken)
        return AuthPayload(
            accessToken = result.accessToken,
            refreshToken = result.refreshToken,
            user = result.user
        )
    }

    // ========== Device Management ==========

    @MutationMapping
    @PreAuthorize("isAuthenticated()")
    fun registerDevice(@Argument input: RegisterDeviceInput): DeviceRegistration {
        val userId = extractUserId()
        val result = deviceService.registerDevice(
            name = input.name,
            userId = userId,
            platform = input.platform,
            capabilities = input.capabilities.toSet()
        )
        return DeviceRegistration(
            device = result.device,
            apiKey = result.apiKey
        )
    }

    @MutationMapping
    @PreAuthorize("hasRole('ADMIN')")
    fun updateDeviceStatus(
        @Argument deviceId: String,
        @Argument status: DeviceStatus
    ): Device {
        return deviceService.updateDeviceStatus(deviceId, status)
    }

    @MutationMapping
    @PreAuthorize("hasRole('ADMIN')")
    fun deleteDevice(@Argument deviceId: String): Boolean {
        return deviceService.deleteDevice(deviceId)
    }

    // ========== Build Management ==========

    @MutationMapping
    @PreAuthorize("hasRole('ADMIN')")
    fun triggerBuild(@Argument input: TriggerBuildInput): BuildJob {
        val userId = extractUserId()
        return buildJobService.triggerBuild(
            component = input.component,
            branch = input.branch,
            triggeredBy = userId
        )
    }

    @MutationMapping
    @PreAuthorize("hasRole('ADMIN')")
    fun cancelBuild(@Argument buildJobId: String): Boolean {
        return buildJobService.cancelBuild(buildJobId)
    }

    // ========== Flash Management ==========

    @MutationMapping
    @PreAuthorize("isAuthenticated()")
    fun createFlashJob(@Argument input: CreateFlashJobInput): FlashJob {
        return flashJobService.createFlashJob(
            buildJobId = input.buildJobId,
            deviceId = input.deviceId,
            targetDevice = input.targetDevice,
            priority = input.priority
        )
    }

    @MutationMapping
    @PreAuthorize("isAuthenticated()")
    fun cancelFlashJob(@Argument flashJobId: String): Boolean {
        return flashJobService.cancelFlashJob(flashJobId)
    }

    // ========== Agent Operations ==========

    @MutationMapping
    fun deviceLogin(@Argument apiKey: String): AuthPayload {
        val result = deviceService.deviceLogin(apiKey)
        return AuthPayload(
            accessToken = result.token,
            refreshToken = result.token,  // Device tokens don't expire quickly
            device = result.device
        )
    }

    @MutationMapping
    @PreAuthorize("hasAuthority('SCOPE_DEVICE')")
    fun claimFlashJob(@Argument flashJobId: String): FlashJob {
        val deviceId = extractDeviceId()
        return flashJobService.claimFlashJob(flashJobId, deviceId)
    }

    @MutationMapping
    @PreAuthorize("hasAuthority('SCOPE_DEVICE')")
    fun updateFlashStatus(
        @Argument flashJobId: String,
        @Argument status: JobStatus
    ): FlashJob {
        return flashJobService.updateFlashStatus(flashJobId, status)
    }

    @MutationMapping
    @PreAuthorize("hasAuthority('SCOPE_DEVICE')")
    fun completeFlashJob(
        @Argument flashJobId: String,
        @Argument success: Boolean,
        @Argument errorMessage: String?,
        @Argument duration: Long
    ): FlashJob {
        val flashJob = flashJobService.completeFlashJob(flashJobId, success)

        // Record flash history
        flashHistoryService.recordFlashResult(
            flashJobId = flashJob.id,
            result = if (success) FlashResult.SUCCESS else FlashResult.FAILED,
            duration = duration,
            errorMessage = errorMessage
        )

        return flashJob
    }

    @MutationMapping
    @PreAuthorize("hasAuthority('SCOPE_DEVICE')")
    fun updateDeviceHeartbeat(): Device {
        val deviceId = extractDeviceId()
        return deviceService.updateDeviceHeartbeat(deviceId)
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
}
