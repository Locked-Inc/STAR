package com.star.flashmanager.security

import jakarta.servlet.FilterChain
import jakarta.servlet.http.HttpServletRequest
import jakarta.servlet.http.HttpServletResponse
import mu.KotlinLogging
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken
import org.springframework.security.core.context.SecurityContextHolder
import org.springframework.security.web.authentication.WebAuthenticationDetailsSource
import org.springframework.stereotype.Component
import org.springframework.web.filter.OncePerRequestFilter

private val logger = KotlinLogging.logger {}

@Component
class JwtAuthenticationFilter(
    private val jwtTokenProvider: JwtTokenProvider
) : OncePerRequestFilter() {

    @Suppress("TooGenericExceptionCaught", "SwallowedException")
    override fun doFilterInternal(
        request: HttpServletRequest,
        response: HttpServletResponse,
        filterChain: FilterChain
    ) {
        try {
            val jwt = extractJwtFromRequest(request)

            if (jwt != null) {
                try {
                    // Try to validate as user access token first
                    val payload = jwtTokenProvider.validateAccessToken(jwt)
                    val userPrincipal = UserPrincipal(
                        id = payload.userId,
                        email = payload.email,
                        role = payload.role
                    )

                    val authentication = UsernamePasswordAuthenticationToken(
                        userPrincipal,
                        null,
                        userPrincipal.authorities
                    )
                    authentication.details = WebAuthenticationDetailsSource().buildDetails(request)
                    SecurityContextHolder.getContext().authentication = authentication
                } catch (e: Exception) {
                    // Try to validate as device token
                    try {
                        val devicePayload = jwtTokenProvider.validateDeviceToken(jwt)
                        val devicePrincipal = DevicePrincipal(deviceId = devicePayload.deviceId)

                        val authentication = UsernamePasswordAuthenticationToken(
                            devicePrincipal,
                            null,
                            devicePrincipal.authorities
                        )
                        authentication.details = WebAuthenticationDetailsSource().buildDetails(request)
                        SecurityContextHolder.getContext().authentication = authentication
                    } catch (deviceEx: Exception) {
                        logger.debug { "Invalid JWT token: ${deviceEx.message}" }
                    }
                }
            }
        } catch (e: Exception) {
            logger.error { "Could not set user authentication in security context: ${e.message}" }
        }

        filterChain.doFilter(request, response)
    }

    private fun extractJwtFromRequest(request: HttpServletRequest): String? {
        val bearerToken = request.getHeader("Authorization")
        val bearerPrefix = "Bearer "
        return if (bearerToken != null && bearerToken.startsWith(bearerPrefix)) {
            bearerToken.substring(bearerPrefix.length)
        } else {
            null
        }
    }
}
