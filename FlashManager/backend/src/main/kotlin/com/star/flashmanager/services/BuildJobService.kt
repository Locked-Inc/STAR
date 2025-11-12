package com.star.flashmanager.services

import com.star.flashmanager.domain.entities.BuildJob
import com.star.flashmanager.domain.enums.Component
import com.star.flashmanager.domain.enums.JobStatus
import com.star.flashmanager.exception.ResourceNotFoundException
import com.star.flashmanager.repositories.BuildJobRepository
import com.star.flashmanager.services.build.BuildExecutor
import org.springframework.stereotype.Service
import org.springframework.transaction.annotation.Transactional
import java.util.UUID

@Service
@Transactional
class BuildJobService(
    private val buildJobRepository: BuildJobRepository,
    private val buildExecutor: BuildExecutor
) {

    fun triggerBuild(component: Component, branch: String?, triggeredBy: String): BuildJob {
        val buildJob = BuildJob(
            id = UUID.randomUUID().toString(),
            component = component,
            branch = branch ?: "main",
            status = JobStatus.PENDING,
            triggeredBy = triggeredBy
        )

        val savedJob = buildJobRepository.save(buildJob)

        // Trigger actual build process asynchronously
        buildExecutor.executeBuild(savedJob)

        return savedJob
    }

    fun getBuildJobById(id: String): BuildJob {
        return buildJobRepository.findById(id)
            .orElseThrow { ResourceNotFoundException("Build job not found with id: $id") }
    }

    fun updateBuildStatus(id: String, status: JobStatus): BuildJob {
        val buildJob = getBuildJobById(id)
        buildJob.status = status
        return buildJobRepository.save(buildJob)
    }

    fun cancelBuild(id: String): Boolean {
        val buildJob = getBuildJobById(id)

        check(buildJob.status != JobStatus.COMPLETED && buildJob.status != JobStatus.FAILED) {
            "Cannot cancel build in ${buildJob.status} state"
        }

        buildJob.status = JobStatus.FAILED
        buildJobRepository.save(buildJob)

        // Note: Actually cancel the running build process

        return true
    }

    fun setBuildArtifact(id: String, binaryPath: String): BuildJob {
        val buildJob = getBuildJobById(id)
        buildJob.binaryPath = binaryPath
        return buildJobRepository.save(buildJob)
    }
}
