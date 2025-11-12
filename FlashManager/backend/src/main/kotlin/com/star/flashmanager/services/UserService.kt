package com.star.flashmanager.services

import com.star.flashmanager.domain.entities.User
import com.star.flashmanager.domain.enums.UserRole
import com.star.flashmanager.exception.ConflictException
import com.star.flashmanager.exception.ResourceNotFoundException
import com.star.flashmanager.repositories.UserRepository
import org.springframework.security.crypto.password.PasswordEncoder
import org.springframework.stereotype.Service
import org.springframework.transaction.annotation.Transactional
import java.util.UUID

@Service
@Transactional
class UserService(
    private val userRepository: UserRepository,
    private val passwordEncoder: PasswordEncoder
) {

    fun getUserById(id: String): User {
        return userRepository.findById(id)
            .orElseThrow { ResourceNotFoundException("User not found with id: $id") }
    }

    fun getUserByEmail(email: String): User? {
        return userRepository.findByEmail(email)
    }

    fun getAllUsers(): List<User> {
        return userRepository.findAll()
    }

    fun createUser(email: String, password: String, role: UserRole = UserRole.USER): User {
        if (userRepository.existsByEmail(email)) {
            throw ConflictException("User with email $email already exists")
        }

        val user = User(
            id = UUID.randomUUID().toString(),
            email = email,
            passwordHash = passwordEncoder.encode(password),
            role = role
        )

        return userRepository.save(user)
    }

    fun verifyPassword(user: User, password: String): Boolean {
        return passwordEncoder.matches(password, user.passwordHash)
    }
}
