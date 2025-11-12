package com.star.flashmanager.services

import com.star.flashmanager.domain.entities.FlashHistory
import com.star.flashmanager.domain.enums.FlashResult
import com.star.flashmanager.repositories.FlashHistoryRepository
import org.springframework.stereotype.Service
import org.springframework.transaction.annotation.Transactional
import java.util.UUID

@Service
@Transactional
class FlashHistoryService(
    private val flashHistoryRepository: FlashHistoryRepository
) {

    fun recordFlashResult(
        flashJobId: String,
        result: FlashResult,
        duration: Long,
        errorMessage: String? = null,
        flashOutput: String? = null
    ): FlashHistory {
        val flashHistory = FlashHistory(
            id = UUID.randomUUID().toString(),
            flashJobId = flashJobId,
            result = result,
            duration = duration,
            errorMessage = errorMessage,
            flashOutput = flashOutput
        )

        return flashHistoryRepository.save(flashHistory)
    }

    fun getFlashHistory(limit: Int? = null): List<FlashHistory> {
        val history = flashHistoryRepository.findAll()
            .sortedByDescending { it.createdAt }

        return if (limit != null) {
            history.take(limit)
        } else {
            history
        }
    }

    fun getFlashHistoryByFlashJobId(flashJobId: String): FlashHistory? {
        return flashHistoryRepository.findByFlashJobId(flashJobId)
    }

    fun getFlashHistoryByResult(result: FlashResult, limit: Int? = null): List<FlashHistory> {
        val history = flashHistoryRepository.findByResult(result)
            .sortedByDescending { it.createdAt }

        return if (limit != null) {
            history.take(limit)
        } else {
            history
        }
    }
}
