package com.star.flashmanager.repositories

import com.star.flashmanager.domain.entities.BuildJob
import com.star.flashmanager.domain.enums.Component
import com.star.flashmanager.domain.enums.JobStatus
import org.springframework.data.jpa.repository.JpaRepository
import org.springframework.data.jpa.repository.Query
import org.springframework.data.repository.query.Param
import org.springframework.stereotype.Repository

@Repository
interface BuildJobRepository : JpaRepository<BuildJob, String> {
    fun findByTriggeredBy(userId: String): List<BuildJob>
    fun findByStatus(status: JobStatus): List<BuildJob>
    fun findByComponent(component: Component): List<BuildJob>

    @Query("""
        SELECT b FROM BuildJob b
        WHERE (:status IS NULL OR b.status = :status)
        AND (:component IS NULL OR b.component = :component)
        ORDER BY b.createdAt DESC
    """)
    fun findByFilters(
        @Param("status") status: JobStatus?,
        @Param("component") component: Component?
    ): List<BuildJob>
}
