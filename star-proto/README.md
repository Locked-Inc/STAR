# star-proto - Protocol Buffers for STAR Project

This directory contains the Protocol Buffers definitions and generated code for the STAR distributed robotics platform.

## Quick Start

### Generate Protocol Buffers Code

From the **workspace root** (`/workspaces/STAR`), run:

```bash
make proto-gen
```

This will:
1. Generate code for all targets (Go, C/nanopb, C++)
2. Initialize the Go module in `gen/go/` if needed
3. Download Go dependencies with `go mod tidy`
4. Synchronize the Go workspace

### Manual Generation (if not using Makefile)

```bash
# From workspace root
cd star-proto

# Generate protobuf code
buf generate proto/ --template buf.gen.yaml

# Setup Go module (first time or after cleaning)
cd gen/go
go mod init github.com/Locked-Inc/star-proto/gen/go
go mod tidy

# Sync workspace
cd /workspaces/STAR
go work sync
```

## Directory Structure

```
star-proto/
+-- proto/              # Protocol Buffer definitions (.proto files)
|   +-- star/v1/        # STAR v1 API
+-- gen/                # Generated code (gitignored)
|   +-- go/             # Go gRPC/protobuf code
|   +-- nanopb/         # C code for star-rx72n-firmware
|   +-- cpp/            # C++ code for star-ros2
+-- nanopb/             # nanopb configuration files
+-- tests/              # Protocol buffer tests
    +-- go/             # Go test cases
```

## Linting and Formatting

```bash
# Lint proto files
buf lint proto/

# Format proto files
buf format --diff proto/
buf format --write proto/
```

## After Fresh Clone

When cloning the repository for the first time, you **must** run:

```bash
make proto-gen
```

This generates the code and sets up the Go module dependency chain required by `go.work`.

## Troubleshooting

### Error: "directory ./star-proto/gen/go does not contain a module"

This means the Go module wasn't initialized after code generation. Run:

```bash
make proto-gen-go
```

### Buf command not found

Install buf CLI:
- macOS: `brew install bufbuild/buf/buf`
- Linux: See https://buf.build/docs/installation

In the dev container, buf is pre-installed.

## References

- [Protocol Buffers Language Guide](https://protobuf.dev/programming-guides/proto3/)
- [Buf Documentation](https://buf.build/docs)
- [gRPC Go Guide](https://grpc.io/docs/languages/go/quickstart/)
