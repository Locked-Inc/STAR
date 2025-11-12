package com.star.flashmanager.domain.entities

import com.star.flashmanager.domain.enums.LogLevel
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
import jakarta.persistence.Table
import org.springframework.data.jpa.domain.support.AuditingEntityListener
import java.time.OffsetDateTime
import java.util.UUID

@Entity
@Table(
    name = "build_logs",
    indexes = [
        Index(name = "idx_build_log_build_job", columnList = "build_job_id"),
        Index(name = "idx_build_log_timestamp", columnList = "timestamp")
    ]
)
@EntityListeners(AuditingEntityListener::class)
data class BuildLog(
    @Id
    val id: String = UUID.randomUUID().toString(),

    @Column(name = "build_job_id", nullable = false)
    val buildJobId: String,

    @Column(nullable = false)
    var timestamp: OffsetDateTime = OffsetDateTime.now(),

    @Column(nullable = false, columnDefinition = "TEXT")
    var line: String,

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    var level: LogLevel = LogLevel.INFO,

    // Relations
    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "build_job_id", insertable = false, updatable = false)
    val buildJob: BuildJob? = null
)
