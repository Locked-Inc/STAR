;;
;; gpio_test/vectors.S -- Minimal relocatable interrupt vector table.
;;
;; Only two vectors are used:
;;   27 -- SWINT  (ThreadX preemption, IER[3] bit 3)
;;   28 -- CMT0   CMI0 (ThreadX 100 Hz tick)
;;
;; Everything else points to _default_isr (infinite loop) so an unexpected
;; interrupt is easy to spot in a debugger.
;;

    .extern __tx_thread_context_save
    .extern __tx_thread_context_restore
    .extern _cmt0_isr

;; ---------------------------------------------------------------------------
;; SWINT handler -- used by ThreadX for ISR-context preemption.
;; ---------------------------------------------------------------------------
    .section .text
    .global  _swint_handler
_swint_handler:
    PUSH    R2
    PUSH    R1
    BSR     __tx_thread_context_save
    BRA     __tx_thread_context_restore

;; ---------------------------------------------------------------------------
;; Default ISR -- traps any unexpected interrupt.
;; ---------------------------------------------------------------------------
    .global _default_isr
_default_isr:
    bra     _default_isr

;; ---------------------------------------------------------------------------
;; Relocatable vector table -- 256 entries (1024 bytes).
;; Only vectors 27 (SWINT) and 28 (CMT0) are wired; rest are _default_isr.
;; ---------------------------------------------------------------------------
    .section .rvectors, "a"
    .global  _rvectors_start
_rvectors_start:
    ;; 0..26
    .long   _default_isr    ;; 0  BUSERR
    .long   _default_isr    ;; 1
    .long   _default_isr    ;; 2  FIFERR
    .long   _default_isr    ;; 3
    .long   _default_isr    ;; 4
    .long   _default_isr    ;; 5  FRDYI
    .long   _default_isr    ;; 6
    .long   _default_isr    ;; 7
    .long   _default_isr    ;; 8
    .long   _default_isr    ;; 9
    .long   _default_isr    ;; 10
    .long   _default_isr    ;; 11
    .long   _default_isr    ;; 12
    .long   _default_isr    ;; 13
    .long   _default_isr    ;; 14
    .long   _default_isr    ;; 15
    .long   _default_isr    ;; 16
    .long   _default_isr    ;; 17
    .long   _default_isr    ;; 18
    .long   _default_isr    ;; 19
    .long   _default_isr    ;; 20
    .long   _default_isr    ;; 21
    .long   _default_isr    ;; 22
    .long   _default_isr    ;; 23
    .long   _default_isr    ;; 24
    .long   _default_isr    ;; 25
    .long   _default_isr    ;; 26
    .long   _swint_handler  ;; 27 SWINT
    .long   _cmt0_isr       ;; 28 CMT0 CMI0
    ;; 29..255
    .long   _default_isr    ;; 29
    .long   _default_isr    ;; 30
    .long   _default_isr    ;; 31
    .long   _default_isr    ;; 32
    .long   _default_isr    ;; 33
    .long   _default_isr    ;; 34
    .long   _default_isr    ;; 35
    .long   _default_isr    ;; 36
    .long   _default_isr    ;; 37
    .long   _default_isr    ;; 38
    .long   _default_isr    ;; 39
    .long   _default_isr    ;; 40
    .long   _default_isr    ;; 41
    .long   _default_isr    ;; 42
    .long   _default_isr    ;; 43
    .long   _default_isr    ;; 44
    .long   _default_isr    ;; 45
    .long   _default_isr    ;; 46
    .long   _default_isr    ;; 47
    .long   _default_isr    ;; 48
    .long   _default_isr    ;; 49
    .long   _default_isr    ;; 50
    .long   _default_isr    ;; 51
    .long   _default_isr    ;; 52
    .long   _default_isr    ;; 53
    .long   _default_isr    ;; 54
    .long   _default_isr    ;; 55
    .long   _default_isr    ;; 56
    .long   _default_isr    ;; 57
    .long   _default_isr    ;; 58
    .long   _default_isr    ;; 59
    .long   _default_isr    ;; 60
    .long   _default_isr    ;; 61
    .long   _default_isr    ;; 62
    .long   _default_isr    ;; 63
    .long   _default_isr    ;; 64
    .long   _default_isr    ;; 65
    .long   _default_isr    ;; 66
    .long   _default_isr    ;; 67
    .long   _default_isr    ;; 68
    .long   _default_isr    ;; 69
    .long   _default_isr    ;; 70
    .long   _default_isr    ;; 71
    .long   _default_isr    ;; 72
    .long   _default_isr    ;; 73
    .long   _default_isr    ;; 74
    .long   _default_isr    ;; 75
    .long   _default_isr    ;; 76
    .long   _default_isr    ;; 77
    .long   _default_isr    ;; 78
    .long   _default_isr    ;; 79
    .long   _default_isr    ;; 80
    .long   _default_isr    ;; 81
    .long   _default_isr    ;; 82
    .long   _default_isr    ;; 83
    .long   _default_isr    ;; 84
    .long   _default_isr    ;; 85
    .long   _default_isr    ;; 86
    .long   _default_isr    ;; 87
    .long   _default_isr    ;; 88
    .long   _default_isr    ;; 89
    .long   _default_isr    ;; 90
    .long   _default_isr    ;; 91
    .long   _default_isr    ;; 92
    .long   _default_isr    ;; 93
    .long   _default_isr    ;; 94
    .long   _default_isr    ;; 95
    .long   _default_isr    ;; 96
    .long   _default_isr    ;; 97
    .long   _default_isr    ;; 98
    .long   _default_isr    ;; 99
    .long   _default_isr    ;; 100
    .long   _default_isr    ;; 101
    .long   _default_isr    ;; 102
    .long   _default_isr    ;; 103
    .long   _default_isr    ;; 104
    .long   _default_isr    ;; 105
    .long   _default_isr    ;; 106
    .long   _default_isr    ;; 107
    .long   _default_isr    ;; 108
    .long   _default_isr    ;; 109
    .long   _default_isr    ;; 110
    .long   _default_isr    ;; 111
    .long   _default_isr    ;; 112
    .long   _default_isr    ;; 113
    .long   _default_isr    ;; 114
    .long   _default_isr    ;; 115
    .long   _default_isr    ;; 116
    .long   _default_isr    ;; 117
    .long   _default_isr    ;; 118
    .long   _default_isr    ;; 119
    .long   _default_isr    ;; 120
    .long   _default_isr    ;; 121
    .long   _default_isr    ;; 122
    .long   _default_isr    ;; 123
    .long   _default_isr    ;; 124
    .long   _default_isr    ;; 125
    .long   _default_isr    ;; 126
    .long   _default_isr    ;; 127
    .long   _default_isr    ;; 128
    .long   _default_isr    ;; 129
    .long   _default_isr    ;; 130
    .long   _default_isr    ;; 131
    .long   _default_isr    ;; 132
    .long   _default_isr    ;; 133
    .long   _default_isr    ;; 134
    .long   _default_isr    ;; 135
    .long   _default_isr    ;; 136
    .long   _default_isr    ;; 137
    .long   _default_isr    ;; 138
    .long   _default_isr    ;; 139
    .long   _default_isr    ;; 140
    .long   _default_isr    ;; 141
    .long   _default_isr    ;; 142
    .long   _default_isr    ;; 143
    .long   _default_isr    ;; 144
    .long   _default_isr    ;; 145
    .long   _default_isr    ;; 146
    .long   _default_isr    ;; 147
    .long   _default_isr    ;; 148
    .long   _default_isr    ;; 149
    .long   _default_isr    ;; 150
    .long   _default_isr    ;; 151
    .long   _default_isr    ;; 152
    .long   _default_isr    ;; 153
    .long   _default_isr    ;; 154
    .long   _default_isr    ;; 155
    .long   _default_isr    ;; 156
    .long   _default_isr    ;; 157
    .long   _default_isr    ;; 158
    .long   _default_isr    ;; 159
    .long   _default_isr    ;; 160
    .long   _default_isr    ;; 161
    .long   _default_isr    ;; 162
    .long   _default_isr    ;; 163
    .long   _default_isr    ;; 164
    .long   _default_isr    ;; 165
    .long   _default_isr    ;; 166
    .long   _default_isr    ;; 167
    .long   _default_isr    ;; 168
    .long   _default_isr    ;; 169
    .long   _default_isr    ;; 170
    .long   _default_isr    ;; 171
    .long   _default_isr    ;; 172
    .long   _default_isr    ;; 173
    .long   _default_isr    ;; 174
    .long   _default_isr    ;; 175
    .long   _default_isr    ;; 176
    .long   _default_isr    ;; 177
    .long   _default_isr    ;; 178
    .long   _default_isr    ;; 179
    .long   _default_isr    ;; 180
    .long   _default_isr    ;; 181
    .long   _default_isr    ;; 182
    .long   _default_isr    ;; 183
    .long   _default_isr    ;; 184
    .long   _default_isr    ;; 185
    .long   _default_isr    ;; 186
    .long   _default_isr    ;; 187
    .long   _default_isr    ;; 188
    .long   _default_isr    ;; 189
    .long   _default_isr    ;; 190
    .long   _default_isr    ;; 191
    .long   _default_isr    ;; 192
    .long   _default_isr    ;; 193
    .long   _default_isr    ;; 194
    .long   _default_isr    ;; 195
    .long   _default_isr    ;; 196
    .long   _default_isr    ;; 197
    .long   _default_isr    ;; 198
    .long   _default_isr    ;; 199
    .long   _default_isr    ;; 200
    .long   _default_isr    ;; 201
    .long   _default_isr    ;; 202
    .long   _default_isr    ;; 203
    .long   _default_isr    ;; 204
    .long   _default_isr    ;; 205
    .long   _default_isr    ;; 206
    .long   _default_isr    ;; 207
    .long   _default_isr    ;; 208
    .long   _default_isr    ;; 209
    .long   _default_isr    ;; 210
    .long   _default_isr    ;; 211
    .long   _default_isr    ;; 212
    .long   _default_isr    ;; 213
    .long   _default_isr    ;; 214
    .long   _default_isr    ;; 215
    .long   _default_isr    ;; 216
    .long   _default_isr    ;; 217
    .long   _default_isr    ;; 218
    .long   _default_isr    ;; 219
    .long   _default_isr    ;; 220
    .long   _default_isr    ;; 221
    .long   _default_isr    ;; 222
    .long   _default_isr    ;; 223
    .long   _default_isr    ;; 224
    .long   _default_isr    ;; 225
    .long   _default_isr    ;; 226
    .long   _default_isr    ;; 227
    .long   _default_isr    ;; 228
    .long   _default_isr    ;; 229
    .long   _default_isr    ;; 230
    .long   _default_isr    ;; 231
    .long   _default_isr    ;; 232
    .long   _default_isr    ;; 233
    .long   _default_isr    ;; 234
    .long   _default_isr    ;; 235
    .long   _default_isr    ;; 236
    .long   _default_isr    ;; 237
    .long   _default_isr    ;; 238
    .long   _default_isr    ;; 239
    .long   _default_isr    ;; 240
    .long   _default_isr    ;; 241
    .long   _default_isr    ;; 242
    .long   _default_isr    ;; 243
    .long   _default_isr    ;; 244
    .long   _default_isr    ;; 245
    .long   _default_isr    ;; 246
    .long   _default_isr    ;; 247
    .long   _default_isr    ;; 248
    .long   _default_isr    ;; 249
    .long   _default_isr    ;; 250
    .long   _default_isr    ;; 251
    .long   _default_isr    ;; 252
    .long   _default_isr    ;; 253
    .long   _default_isr    ;; 254
    .long   _default_isr    ;; 255

    .end
