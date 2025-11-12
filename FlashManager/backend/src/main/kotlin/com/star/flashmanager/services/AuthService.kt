package com.star.flashmanager.services

import com.star.flashmanager.domain.entities.User
import com.star.flashmanager.exception.UnauthorizedException
import com.star.flashmanager.security.JwtTokenProvider
import org.springframework.stereotype.Service
import org.springframework.transaction.annotation.Transactional

@Service
@Transactional
class AuthService(
    private val userService: UserService,
    private val jwtTokenProvider: JwtTokenProvider
) {

    fun register(email: String, password: String): AuthResult {
        val user = userService.createUser(email, password)
        return generateAuthResult(user)
    }

    fun login(email: String, password: String): AuthResult {
        val user = userService.getUserByEmail(email)
            ?: throw UnauthorizedException("Invalid email or password")

        if (!userService.verifyPassword(user, password)) {
            throw UnauthorizedException("Invalid email or password")
        }

        return generateAuthResult(user)
    }

    fun refreshToken(refreshToken: String): AuthResult {
        val payload = jwtTokenProvider.validateRefreshToken(refreshToken)
        val user = userService.getUserById(payload.userId)
        return generateAuthResult(user)
    }

    private fun generateAuthResult(user: User): AuthResult {
        val accessToken = jwtTokenProvider.generateAccessToken(
            userId = user.id,
            email = user.email,
            role = user.role.name
        )

        val refreshToken = jwtTokenProvider.generateRefreshToken(userId = user.id)

        return AuthResult(
            accessToken = accessToken,
            refreshToken = refreshToken,
            user = user
        )
    }

    data class AuthResult(
        val accessToken: String,
        val refreshToken: String,
        val user: User
    )
}
