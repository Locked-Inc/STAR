package com.star.flashmanager.repositories

import com.star.flashmanager.domain.entities.BuildLog
import org.springframework.data.domain.Pageable
import org.springframework.data.jpa.repository.JpaRepository
import org.springframework.data.jpa.repository.Query
import org.springframework.data.repository.query.Param
import org.springframework.stereotype.Repository

@Repository
interface BuildLogRepository : JpaRepository<BuildLog, String> {
    fun findByBuildJobIdOrderByTimestampAsc(buildJobId: String): List<BuildLog>

    @Query("""
        SELECT l FROM BuildLog l
        WHERE l.buildJobId = :buildJobId
        ORDER BY l.timestamp ASC
    """)
    fun findByBuildJobIdWithPagination(
        @Param("buildJobId") buildJobId: String,
        pageable: Pageable
    ): List<BuildLog>
}
