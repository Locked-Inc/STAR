// build.gradle.kts - Kotlin Protobuf Serialization Tests
// STAR Project - Texas A&M University
//
// Using JUnit with gRPC testing patterns:
// https://grpc.io/blog/graceful-cleanup-junit-tests/

plugins {
    kotlin("jvm") version "1.9.22"
}

group = "com.star.proto"
version = "1.0.0"

repositories {
    mavenCentral()
}

val grpcVersion = "1.60.0"
val grpcKotlinVersion = "1.4.1"
val protobufVersion = "3.25.3"
val junitVersion = "5.10.1"

dependencies {
    // Protobuf runtime
    implementation("com.google.protobuf:protobuf-kotlin:$protobufVersion")
    implementation("com.google.protobuf:protobuf-java-util:$protobufVersion")

    // gRPC runtime
    implementation("io.grpc:grpc-kotlin-stub:$grpcKotlinVersion")
    implementation("io.grpc:grpc-protobuf:$grpcVersion")
    implementation("io.grpc:grpc-stub:$grpcVersion")
    runtimeOnly("io.grpc:grpc-netty-shaded:$grpcVersion")

    // Coroutines (required by generated gRPC-Kotlin code)
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.7.3")

    // JUnit 5 testing
    testImplementation(kotlin("test"))
    testImplementation("org.junit.jupiter:junit-jupiter-api:$junitVersion")
    testImplementation("org.junit.jupiter:junit-jupiter-params:$junitVersion")
    testRuntimeOnly("org.junit.jupiter:junit-jupiter-engine:$junitVersion")

    // gRPC testing - GrpcCleanupRule, InProcessServer, InProcessChannel
    testImplementation("io.grpc:grpc-testing:$grpcVersion")
    testImplementation("io.grpc:grpc-inprocess:$grpcVersion")

    // Coroutines for Kotlin gRPC
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.7.3")

    // Assertions
    testImplementation("org.assertj:assertj-core:3.24.2")
}

// Include generated code from buf generate
sourceSets {
    main {
        java {
            srcDir("../../gen/kotlin")
        }
    }
}

tasks.test {
    useJUnitPlatform()
    testLogging {
        events("passed", "skipped", "failed")
        showStandardStreams = true
    }
}

kotlin {
    jvmToolchain(17)
}
