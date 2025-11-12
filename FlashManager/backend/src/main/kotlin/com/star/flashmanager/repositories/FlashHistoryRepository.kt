package com.star.flashmanager.repositories

import com.star.flashmanager.domain.entities.FlashHistory
import com.star.flashmanager.domain.enums.FlashResult
import org.springframework.data.jpa.repository.JpaRepository
import org.springframework.stereotype.Repository

@Repository
interface FlashHistoryRepository : JpaRepository<FlashHistory, String> {
    fun findByFlashJobId(flashJobId: String): FlashHistory?
    fun findByResult(result: FlashResult): List<FlashHistory>
}
