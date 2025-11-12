package com.star.flashmanager.domain.entities

import com.star.flashmanager.domain.enums.DeviceCapability
import com.star.flashmanager.domain.enums.DeviceStatus
import com.star.flashmanager.domain.enums.Platform
import jakarta.persistence.CollectionTable
import jakarta.persistence.Column
import jakarta.persistence.ElementCollection
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
import org.springframework.data.annotation.CreatedDate
import org.springframework.data.annotation.LastModifiedDate
import org.springframework.data.jpa.domain.support.AuditingEntityListener
import java.time.OffsetDateTime
import java.util.UUID

@Entity
@Table(
    name = "devices",
    indexes = [
        Index(name = "idx_device_user", columnList = "user_id"),
        Index(name = "idx_device_status", columnList = "status")
    ]
)
@EntityListeners(AuditingEntityListener::class)
data class Device(
    @Id
    val id: String = UUID.randomUUID().toString(),

    @Column(nullable = false)
    var name: String,

    @Column(name = "api_key_hash", nullable = false)
    var apiKeyHash: String,

    @Column(name = "user_id", nullable = false)
    val userId: String,

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    var status: DeviceStatus = DeviceStatus.OFFLINE,

    @Column(name = "last_seen")
    var lastSeen: OffsetDateTime? = null,

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    var platform: Platform,

    @ElementCollection(targetClass = DeviceCapability::class, fetch = FetchType.EAGER)
    @CollectionTable(name = "device_capabilities", joinColumns = [JoinColumn(name = "device_id")])
    @Column(name = "capability")
    @Enumerated(EnumType.STRING)
    var capabilities: MutableSet<DeviceCapability> = mutableSetOf(),

    @CreatedDate
    @Column(name = "created_at", updatable = false)
    var createdAt: OffsetDateTime? = null,

    @LastModifiedDate
    @Column(name = "updated_at")
    var updatedAt: OffsetDateTime? = null,

    // Relations
    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "user_id", insertable = false, updatable = false)
    val user: User? = null
)
