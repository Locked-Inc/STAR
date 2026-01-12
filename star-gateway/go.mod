module github.com/Locked-Inc/STAR/star-gateway

go 1.24.11

require (
	github.com/Locked-Inc/star-proto/gen/go v0.0.0
	google.golang.org/grpc v1.64.0
	google.golang.org/protobuf v1.34.2
	nhooyr.io/websocket v1.8.17
)

require (
	golang.org/x/net v0.22.0 // indirect
	golang.org/x/sys v0.18.0 // indirect
	golang.org/x/text v0.14.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20240318140521-94a12d6c2237 // indirect
)

replace github.com/Locked-Inc/star-proto/gen/go => ../star-proto/gen/go
