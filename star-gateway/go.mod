module github.com/Locked-Inc/STAR/star-gateway

go 1.25.7

require (
	github.com/Locked-Inc/star-proto/gen/go v0.0.0
	github.com/fsnotify/fsnotify v1.7.0
	go.bug.st/serial v1.6.4
	google.golang.org/grpc v1.79.1
	google.golang.org/protobuf v1.36.11
	nhooyr.io/websocket v1.8.17
	periph.io/x/conn/v3 v3.7.2
	periph.io/x/host/v3 v3.8.5
)

require (
	github.com/creack/goselect v0.1.2 // indirect
	github.com/davecgh/go-spew v1.1.2-0.20180830191138-d8f796af33cc // indirect
	github.com/pmezard/go-difflib v1.0.1-0.20181226105442-5d4384ee4fb2 // indirect
	github.com/stretchr/testify v1.11.1 // indirect
	golang.org/x/net v0.50.0 // indirect
	golang.org/x/sys v0.41.0 // indirect
	golang.org/x/text v0.34.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20260209200024-4cfbd4190f57 // indirect
)

replace github.com/Locked-Inc/star-proto/gen/go => ../star-proto/gen/go
