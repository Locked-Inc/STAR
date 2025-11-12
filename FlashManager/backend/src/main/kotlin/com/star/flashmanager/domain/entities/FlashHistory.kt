package com.star.flashmanager.domain.entities

import com.star.flashmanager.domain.enums.FlashResult
import jakarta.persistence.Column
import jakarta.persistence.Entity
import jakarta.persistence.EntityListeners
import jakarta.persistence.EnumType
import jakarta.persistence.Enumerated
import jakarta.persistence.FetchType
import jakarta.persistence.Id
import jakarta.persistence.Index
import jakarta.persistence.JoinColumn
import jakarta.persistence.OneToOne
import jakarta.persistence.Table
import org.springframework.data.annotation.CreatedDate
import org.springframework.data.jpa.domain.support.AuditingEntityListener
import java.time.OffsetDateTime
import java.util.UUID

@Entity
@Table(
    name = "flash_history",
    indexes = [
        Index(name = "idx_flash_history_flash_job", columnList = "flash_job_id"),
        Index(name = "idx_flash_history_result", columnList = "result")
    ]
)
@EntityListeners(AuditingEntityListener::class)
data class FlashHistory(
    @Id
    val id: String = UUID.randomUUID().toString(),

    @Column(name = "flash_job_id", nullable = false, unique = true)
    val flashJobId: String,

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    var result: FlashResult,

    @Column(nullable = false)
    var duration: Long,

    @Column(name = "error_message", columnDefinition = "TEXT")
    var errorMessage: String? = null,

    @Column(name = "flash_output", columnDefinition = "TEXT")
    var flashOutput: String? = null,

    @CreatedDate
    @Column(name = "created_at", updatable = false)
    var createdAt: OffsetDateTime? = null,

    // Relations
    @OneToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "flash_job_id", insertable = false, updatable = false)
    val flashJob: FlashJob? = null
)
