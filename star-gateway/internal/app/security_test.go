// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Tests for audit fixes F-01 (require-localhost address enforcement) and
// F-02 (two-key gate for WebSocket origin-checking bypass).

package app

import (
	"io"
	"log/slog"
	"os"
	"testing"
)

// quietLogger returns a no-op slog.Logger. We send everything to io.Discard so
// the test output stays focused on assertions.
func quietLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}

// TestEnforceLocalhostAddr_PassthroughWhenDisabled verifies that when
// require-localhost is false the listenAddr is returned unchanged.
func TestEnforceLocalhostAddr_PassthroughWhenDisabled(t *testing.T) {
	t.Parallel()
	cases := []string{":8080", "0.0.0.0:8080", "192.168.1.10:50051", "[::]:8080"}
	for _, in := range cases {
		got, err := enforceLocalhostAddr(in, false)
		if err != nil {
			t.Errorf("enforceLocalhostAddr(%q,false) unexpected error: %v", in, err)
		}
		if got != in {
			t.Errorf("enforceLocalhostAddr(%q,false) = %q, want unchanged", in, got)
		}
	}
}

// TestEnforceLocalhostAddr_RewritesWildcards verifies that when
// require-localhost is true the wildcard hosts are rewritten to 127.0.0.1.
func TestEnforceLocalhostAddr_RewritesWildcards(t *testing.T) {
	t.Parallel()
	cases := []struct {
		in   string
		want string
	}{
		{":8080", "127.0.0.1:8080"},
		{"0.0.0.0:8080", "127.0.0.1:8080"},
		{"[::]:9000", "127.0.0.1:9000"},
	}
	for _, tc := range cases {
		got, err := enforceLocalhostAddr(tc.in, true)
		if err != nil {
			t.Errorf("enforceLocalhostAddr(%q,true) unexpected error: %v", tc.in, err)
			continue
		}
		if got != tc.want {
			t.Errorf("enforceLocalhostAddr(%q,true) = %q, want %q", tc.in, got, tc.want)
		}
	}
}

// TestEnforceLocalhostAddr_AllowsLoopbackHosts verifies that explicit
// loopback hosts pass through cleanly when require-localhost is on.
func TestEnforceLocalhostAddr_AllowsLoopbackHosts(t *testing.T) {
	t.Parallel()
	cases := []string{"127.0.0.1:8080", "localhost:50051"}
	for _, in := range cases {
		got, err := enforceLocalhostAddr(in, true)
		if err != nil {
			t.Errorf("enforceLocalhostAddr(%q,true) unexpected error: %v", in, err)
			continue
		}
		if got != in {
			t.Errorf("enforceLocalhostAddr(%q,true) = %q, want unchanged", in, got)
		}
	}
}

// TestEnforceLocalhostAddr_RefusesNonLoopback verifies that a non-loopback
// explicit host is rejected when require-localhost is on.
func TestEnforceLocalhostAddr_RefusesNonLoopback(t *testing.T) {
	t.Parallel()
	cases := []string{"192.168.1.10:8080", "10.0.0.1:50051", "example.com:443"}
	for _, in := range cases {
		_, err := enforceLocalhostAddr(in, true)
		if err == nil {
			t.Errorf("enforceLocalhostAddr(%q,true) accepted non-loopback host", in)
		}
	}
}

// TestEnforceLocalhostAddr_NoPort verifies the error path when the input
// is missing a colon and therefore has no port at all.
func TestEnforceLocalhostAddr_NoPort(t *testing.T) {
	t.Parallel()
	if _, err := enforceLocalhostAddr("notaddress", true); err == nil {
		t.Fatal("enforceLocalhostAddr expected error for portless input")
	}
}

// TestResolveStrictOriginChecking_DefaultIsStrict verifies the default
// behaviour with neither env var set: strict origin checking stays on.
func TestResolveStrictOriginChecking_DefaultIsStrict(t *testing.T) {
	t.Setenv(wsStrictOriginEnv, "")
	t.Setenv(insecureDevModeEnv, "")
	// t.Setenv with "" still sets the var; clear it explicitly below.
	if err := unsetEnv(t, wsStrictOriginEnv); err != nil {
		t.Fatal(err)
	}
	if err := unsetEnv(t, insecureDevModeEnv); err != nil {
		t.Fatal(err)
	}
	if got := resolveStrictOriginChecking(quietLogger()); !got {
		t.Errorf("default resolveStrictOriginChecking() = false, want true")
	}
}

// TestResolveStrictOriginChecking_BothKeysDisable verifies that ONLY when
// both keys agree can the bypass actually take effect.
func TestResolveStrictOriginChecking_BothKeysDisable(t *testing.T) {
	t.Setenv(wsStrictOriginEnv, "false")
	t.Setenv(insecureDevModeEnv, "true")
	if got := resolveStrictOriginChecking(quietLogger()); got {
		t.Errorf("resolveStrictOriginChecking() = true with both keys, want false (disabled)")
	}
}

// TestResolveStrictOriginChecking_OnlyWSStrictFalseRefuses verifies that
// setting WS_STRICT_ORIGIN=false alone keeps origin checking ON. This is
// the headline of audit F-02.
func TestResolveStrictOriginChecking_OnlyWSStrictFalseRefuses(t *testing.T) {
	t.Setenv(wsStrictOriginEnv, "false")
	if err := unsetEnv(t, insecureDevModeEnv); err != nil {
		t.Fatal(err)
	}
	if got := resolveStrictOriginChecking(quietLogger()); !got {
		t.Errorf("resolveStrictOriginChecking() with only %s=false = false, want true (refuse to disable)",
			wsStrictOriginEnv)
	}
}

// TestResolveStrictOriginChecking_OnlyDevModeRefuses verifies the symmetric
// case: INSECURE_DEV_MODE=true alone is not enough to disable.
func TestResolveStrictOriginChecking_OnlyDevModeRefuses(t *testing.T) {
	if err := unsetEnv(t, wsStrictOriginEnv); err != nil {
		t.Fatal(err)
	}
	t.Setenv(insecureDevModeEnv, "true")
	if got := resolveStrictOriginChecking(quietLogger()); !got {
		t.Errorf("resolveStrictOriginChecking() with only %s=true = false, want true (refuse to disable)",
			insecureDevModeEnv)
	}
}

// TestResolveStrictOriginChecking_TruthyFalseyVariants exercises the
// canonical truthy/falsey synonyms accepted by isFalsey/isTruthy.
func TestResolveStrictOriginChecking_TruthyFalseyVariants(t *testing.T) {
	cases := []struct {
		ws  string
		dev string
	}{
		{"FALSE", "TRUE"},
		{"0", "1"},
		{" off ", "on"},
		{"no", "yes"},
	}
	for _, tc := range cases {
		t.Setenv(wsStrictOriginEnv, tc.ws)
		t.Setenv(insecureDevModeEnv, tc.dev)
		if got := resolveStrictOriginChecking(quietLogger()); got {
			t.Errorf("resolveStrictOriginChecking(ws=%q,dev=%q) = true, want false (disabled)",
				tc.ws, tc.dev)
		}
	}
}

// unsetEnv removes the given env var for the lifetime of the test. We
// implement this manually because t.Setenv(name, "") still leaves the
// variable defined (set to empty) -- our LookupEnv branches need it
// to be truly unset.
func unsetEnv(t *testing.T, name string) error {
	t.Helper()
	prev, hadPrev := os.LookupEnv(name)
	if err := os.Unsetenv(name); err != nil {
		return err
	}
	t.Cleanup(func() {
		if hadPrev {
			_ = os.Setenv(name, prev)
		} else {
			_ = os.Unsetenv(name)
		}
	})
	return nil
}
