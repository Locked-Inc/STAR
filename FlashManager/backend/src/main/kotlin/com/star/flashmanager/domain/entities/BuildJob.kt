package com.star.flashmanager.domain.entities

import com.star.flashmanager.domain.enums.Component
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
import jakarta.persistence.OneToMany
import jakarta.persistence.Table
import org.springframework.data.annotation.CreatedDate
import org.springframework.data.annotation.LastModifiedDate
import org.springframework.data.jpa.domain.support.AuditingEntityListener
import java.time.OffsetDateTime
import java.util.UUID

@Entity
@Table(
    name = "build_jobs",
    indexes = [
        Index(name = "idx_build_job_status", columnList = "status"),
        Index(name = "idx_build_job_component", columnList = "component"),
        Index(name = "idx_build_job_user", columnList = "triggered_by")
    ]
)
@EntityListeners(AuditingEntityListener::class)
data class BuildJob(
    @Id
    val id: String = UUID.randomUUID().toString(),

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    var component: Component,

    @Column(nullable = false)
    var branch: String = "main",

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    var status: JobStatus = JobStatus.PENDING,

    @Column(name = "triggered_by", nullable = false)
    val triggeredBy: String,

    @Column(name = "started_at")
    var startedAt: OffsetDateTime? = null,

    @Column(name = "completed_at")
    var completedAt: OffsetDateTime? = null,

    @Column(name = "binary_path")
    var binaryPath: String? = null,

    @Column(name = "binary_size")
    var binarySize: Long? = null,

    @Column(name = "build_duration")
    var buildDuration: Long? = null,

    @Column(name = "exit_code")
    var exitCode: Int? = null,

    @CreatedDate
    @Column(name = "created_at", updatable = false)
    var createdAt: OffsetDateTime? = null,

    @LastModifiedDate
    @Column(name = "updated_at")
    var updatedAt: OffsetDateTime? = null,

    // Relations
    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "triggered_by", insertable = false, updatable = false)
    val user: User? = null,

    @OneToMany(mappedBy = "buildJob", cascade = [CascadeType.ALL], fetch = FetchType.LAZY)
    val logs: MutableList<BuildLog> = mutableListOf()
)
