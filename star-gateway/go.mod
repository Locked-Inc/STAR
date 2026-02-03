module github.com/Locked-Inc/STAR/star-gateway

go 1.24.12

require (
	github.com/Locked-Inc/star-proto/gen/go v0.0.0
	go.bug.st/serial v1.6.4
	google.golang.org/grpc v1.78.0
	google.golang.org/protobuf v1.36.11
	nhooyr.io/websocket v1.8.17
	periph.io/x/conn/v3 v3.7.2
	periph.io/x/host/v3 v3.8.5
)

require (
	github.com/creack/goselect v0.1.2 // indirect
	github.com/fsnotify/fsnotify v1.9.0 // indirect
	golang.org/x/net v0.47.0 // indirect
	golang.org/x/sys v0.38.0 // indirect
	golang.org/x/text v0.31.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20251029180050-ab9386a59fda // indirect
)

replace github.com/Locked-Inc/star-proto/gen/go => ../star-proto/gen/go
