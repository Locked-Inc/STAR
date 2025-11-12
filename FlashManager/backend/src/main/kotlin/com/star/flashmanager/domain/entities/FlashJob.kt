package com.star.flashmanager.domain.entities

import com.star.flashmanager.domain.enums.JobStatus
import jakarta.persistence.CascadeType
import jakarta.persistence.Column
import jakarta.persistence.Entity
import jakarta.persistence.EntityListeners
import jakarta.persistence.EnumType
import jakarta.persistence.Enumerated
import jakarta.persistence.FetchType
import jakarta.persistence.Id
import jakarta.persistence.Index
import jakarta.persistence.JoinColumn
import jakarta.persistence.ManyToOne
import jakarta.persistence.OneToOne
import jakarta.persistence.Table
import org.springframework.data.annotation.CreatedDate
import org.springframework.data.annotation.LastModifiedDate
import org.springframework.data.jpa.domain.support.AuditingEntityListener
import java.time.OffsetDateTime
import java.util.UUID

@Entity
@Table(
    name = "flash_jobs",
    indexes = [
        Index(name = "idx_flash_job_status", columnList = "status"),
        Index(name = "idx_flash_job_device", columnList = "device_id"),
        Index(name = "idx_flash_job_build", columnList = "build_job_id"),
        Index(name = "idx_flash_job_priority", columnList = "priority")
    ]
)
@EntityListeners(AuditingEntityListener::class)
data class FlashJob(
    @Id
    val id: String = UUID.randomUUID().toString(),

    @Column(name = "build_job_id", nullable = false)
    val buildJobId: String,

    @Column(name = "device_id", nullable = false)
    val deviceId: String,

    @Column(name = "target_device", nullable = false)
    var targetDevice: String,

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    var status: JobStatus = JobStatus.PENDING,

    @Column(nullable = false)
    var priority: Int = 0,

    @Column(name = "claimed_at")
    var claimedAt: OffsetDateTime? = null,

    @Column(name = "completed_at")
    var completedAt: OffsetDateTime? = null,

    @CreatedDate
    @Column(name = "created_at", updatable = false)
    var createdAt: OffsetDateTime? = null,

    @LastModifiedDate
    @Column(name = "updated_at")
    var updatedAt: OffsetDateTime? = null,

    // Relations
    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "build_job_id", insertable = false, updatable = false)
    val buildJob: BuildJob? = null,

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "device_id", insertable = false, updatable = false)
    val device: Device? = null,

    @OneToOne(mappedBy = "flashJob", cascade = [CascadeType.ALL], fetch = FetchType.LAZY)
    val history: FlashHistory? = null
)
