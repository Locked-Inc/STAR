package com.star.flashmanager.security

import io.jsonwebtoken.Claims
import io.jsonwebtoken.Jwts
import io.jsonwebtoken.security.Keys
import org.springframework.beans.factory.annotation.Value
import org.springframework.stereotype.Component
import java.time.Instant
import java.time.temporal.ChronoUnit
import java.util.*
import javax.crypto.SecretKey

@Component
class JwtTokenProvider(
    @Value("\${jwt.secret}")
    private val secretKey: String,
    @Value("\${jwt.expiration}")
    private val accessTokenExpiryMs: Long,
    @Value("\${jwt.refresh-expiration:2592000000}")
    private val refreshTokenExpiryMs: Long
) {

    private val key: SecretKey by lazy {
        Keys.hmacShaKeyFor(secretKey.toByteArray())
    }

    fun generateAccessToken(
        userId: String,
        email: String,
        role: String,
        additionalClaims: Map<String, Any?> = emptyMap()
    ): String {
        val now = Instant.now()
        val expiry = now.plusMillis(accessTokenExpiryMs)

        val builder = Jwts.builder()
            .subject(userId)
            .claim("email", email)
            .claim("role", role)
            .claim("type", "access")
            .issuedAt(Date.from(now))
            .expiration(Date.from(expiry))
            .signWith(key)

        additionalClaims.forEach { (name, value) ->
            if (value != null) {
                builder.claim(name, value)
            }
        }

        return builder.compact()
    }

    fun generateRefreshToken(userId: String, additionalClaims: Map<String, Any?> = emptyMap()): String {
        val now = Instant.now()
        val expiry = now.plusMillis(refreshTokenExpiryMs)

        val builder = Jwts.builder()
            .subject(userId)
            .claim("type", "refresh")
            .issuedAt(Date.from(now))
            .expiration(Date.from(expiry))
            .signWith(key)

        additionalClaims.forEach { (name, value) ->
            if (value != null) {
                builder.claim(name, value)
            }
        }

        return builder.compact()
    }

    @Suppress("MagicNumber")
    fun generateDeviceToken(deviceId: String): String {
        val now = Instant.now()
        val deviceTokenExpiryDays = 30L
        val expiry = now.plus(deviceTokenExpiryDays, ChronoUnit.DAYS)

        return Jwts.builder()
            .subject(deviceId)
            .claim("type", "device")
            .issuedAt(Date.from(now))
            .expiration(Date.from(expiry))
            .signWith(key)
            .compact()
    }

    fun validateAccessToken(token: String): JwtPayload {
        val claims = parseToken(token)
        require(claims.get("type") == "access") { "Invalid token type" }

        return JwtPayload(
            userId = claims.subject,
            email = claims.get("email", String::class.java),
            role = claims.get("role", String::class.java)
        )
    }

    fun validateDeviceToken(token: String): DeviceTokenPayload {
        val claims = parseToken(token)
        require(claims.get("type") == "device") { "Invalid token type" }

        return DeviceTokenPayload(deviceId = claims.subject)
    }

    fun validateRefreshToken(token: String): RefreshTokenPayload {
        val claims = parseToken(token)
        require(claims.get("type") == "refresh") { "Invalid token type" }

        return RefreshTokenPayload(userId = claims.subject)
    }

    private fun parseToken(token: String): Claims {
        return Jwts.parser()
            .verifyWith(key)
            .build()
            .parseSignedClaims(token)
            .payload
    }

    data class JwtPayload(
        val userId: String,
        val email: String,
        val role: String
    )

    data class DeviceTokenPayload(
        val deviceId: String
    )

    data class RefreshTokenPayload(
        val userId: String
    )
}
