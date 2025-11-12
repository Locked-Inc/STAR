package com.star.flashmanager.repositories

import com.star.flashmanager.domain.entities.User
import com.star.flashmanager.domain.enums.UserRole
import org.springframework.data.jpa.repository.JpaRepository
import org.springframework.stereotype.Repository

@Repository
interface UserRepository : JpaRepository<User, String> {
    fun findByEmail(email: String): User?
    fun existsByEmail(email: String): Boolean
    fun findByRole(role: UserRole): List<User>
}
