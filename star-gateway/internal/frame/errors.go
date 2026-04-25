// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package frame

import (
	"errors"
)

// ErrSyncLoss indicates frame synchronization was lost.
var ErrSyncLoss = errors.New("frame sync lost after max search")

// ErrInvalidLength indicates frame length field exceeds limits.
var ErrInvalidLength = errors.New("frame length exceeds maximum")
