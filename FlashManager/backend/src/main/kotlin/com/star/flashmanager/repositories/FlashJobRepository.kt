package com.star.flashmanager.repositories

import com.star.flashmanager.domain.entities.FlashJob
import com.star.flashmanager.domain.enums.JobStatus
import org.springframework.data.jpa.repository.JpaRepository
import org.springframework.data.jpa.repository.Query
import org.springframework.data.repository.query.Param
import org.springframework.stereotype.Repository

@Repository
interface FlashJobRepository : JpaRepository<FlashJob, String> {
    fun findByDeviceId(deviceId: String): List<FlashJob>
    fun findByStatus(status: JobStatus): List<FlashJob>
    fun findByDeviceIdAndStatus(deviceId: String, status: JobStatus): List<FlashJob>

    @Query("""
        SELECT f FROM FlashJob f
        WHERE f.deviceId = :deviceId
        AND f.status = 'PENDING'
        ORDER BY f.priority DESC, f.createdAt ASC
    """)
    fun findPendingJobsForDevice(@Param("deviceId") deviceId: String): List<FlashJob>
}
