package com.star.flashmanager.exception

class ResourceNotFoundException(message: String) : RuntimeException(message)
class AccessDeniedDomainException(message: String) : RuntimeException(message)
class ValidationException(message: String) : RuntimeException(message)
class UnauthorizedException(message: String) : RuntimeException(message)
class ConflictException(message: String) : RuntimeException(message)
