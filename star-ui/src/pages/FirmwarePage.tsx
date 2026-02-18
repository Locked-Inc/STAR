import { useState, useEffect, useRef } from 'react'
import { useGateway } from '@/hooks/useGateway'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Progress } from '@/components/ui/progress'
import { Badge } from '@/components/ui/badge'
import type { FirmwareInfo } from '@/proto/star/v1/firmware_update'
import {
  Upload,
  Info,
  RotateCcw,
  Power,
  CheckCircle,
  AlertTriangle,
  FileUp,
} from 'lucide-react'
import { formatBytes } from '@/lib/utils'

type UploadStatus = 'idle' | 'receiving' | 'validating' | 'complete' | 'failed' | 'aborted'

function stateLabel(status: UploadStatus): { label: string; variant: 'default' | 'info' | 'warning' | 'success' | 'danger' } {
  switch (status) {
    case 'idle': return { label: 'Idle', variant: 'default' }
    case 'receiving': return { label: 'Receiving', variant: 'info' }
    case 'validating': return { label: 'Validating', variant: 'warning' }
    case 'complete': return { label: 'Complete', variant: 'success' }
    case 'failed': return { label: 'Failed', variant: 'danger' }
    case 'aborted': return { label: 'Aborted', variant: 'danger' }
  }
}

const CHUNK_SIZE = 4096

export function FirmwarePage() {
  const { client } = useGateway()
  const [info, setInfo] = useState<FirmwareInfo | null>(null)
  const [loading, setLoading] = useState(true)
  const [selectedFile, setSelectedFile] = useState<File | null>(null)
  const [uploadProgress, setUploadProgress] = useState(0)
  const [uploadStatus, setUploadStatus] = useState<UploadStatus>('idle')
  const [uploadError, setUploadError] = useState('')
  const [rebootDelay, setRebootDelay] = useState(3)
  const [actionState, setActionState] = useState<'idle' | 'rebooting' | 'rolling-back' | 'marking'>('idle')
  const fileInputRef = useRef<HTMLInputElement>(null)
  const abortRef = useRef(false)

  useEffect(() => {
    void loadInfo()
  }, [])

  async function loadInfo() {
    setLoading(true)
    try {
      const fw = await client.getFirmwareInfo()
      setInfo(fw)
    } finally {
      setLoading(false)
    }
  }

  async function startUpload() {
    if (!selectedFile) return
    abortRef.current = false
    setUploadError('')
    setUploadProgress(0)

    try {
      const buf = await selectedFile.arrayBuffer()
      const data = new Uint8Array(buf)

      // Simple CRC-32 placeholder — real implementation would compute proper CRC
      const crc32 = 0
      setUploadStatus('receiving')
      await client.beginFirmwareUpdate(data.length, crc32)

      const totalChunks = Math.ceil(data.length / CHUNK_SIZE)
      for (let i = 0; i < totalChunks; i++) {
        if (abortRef.current) {
          await client.abortFirmwareUpdate()
          setUploadStatus('aborted')
          return
        }
        const start = i * CHUNK_SIZE
        const chunk = data.slice(start, start + CHUNK_SIZE)
        await client.uploadFirmwareChunk(chunk, start)
        setUploadProgress(Math.round(((i + 1) / totalChunks) * 100))
      }

      setUploadStatus('validating')
      await client.finalizeFirmwareUpdate()
      setUploadStatus('complete')
      void loadInfo()
    } catch (e) {
      setUploadError(e instanceof Error ? e.message : 'Upload failed')
      setUploadStatus('failed')
    }
  }

  function abortUpload() {
    abortRef.current = true
  }

  async function handleReboot() {
    setActionState('rebooting')
    try {
      await client.reboot(rebootDelay * 1000)
    } finally {
      setActionState('idle')
    }
  }

  async function handleRollback() {
    setActionState('rolling-back')
    try {
      await client.rollback()
      await loadInfo()
    } finally {
      setActionState('idle')
    }
  }

  async function handleMarkValid() {
    setActionState('marking')
    try {
      await client.markValid()
      await loadInfo()
    } finally {
      setActionState('idle')
    }
  }

  const isUploading = uploadStatus === 'receiving' || uploadStatus === 'validating'
  const { label: stateText, variant: stateVariant } = stateLabel(uploadStatus)

  return (
    <div className="mx-auto max-w-2xl space-y-6">
      {/* Current firmware info */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Info size={14} /> Firmware Information
          </CardTitle>
        </CardHeader>
        <CardContent>
          {loading ? (
            <div className="text-slate-500">Loading…</div>
          ) : info ? (
            <div className="grid grid-cols-2 gap-3 text-sm">
              {[
                { label: 'Version', value: info.version, mono: true },
                { label: 'Git Hash', value: info.gitHash, mono: true },
                { label: 'Build Date', value: info.buildDate, mono: false },
                { label: 'Size', value: formatBytes(info.size), mono: true },
                { label: 'CRC-32', value: `0x${info.crc32.toString(16).toUpperCase()}`, mono: true },
                { label: 'Chip', value: info.chipTarget, mono: false },
                { label: 'IDF Version', value: info.idfVersion, mono: true },
              ].map(({ label, value, mono }) => (
                <div key={label}>
                  <div className="text-xs uppercase tracking-wider text-slate-500">{label}</div>
                  <div className={mono ? 'font-mono text-white' : 'text-white'}>{value}</div>
                </div>
              ))}
            </div>
          ) : (
            <div className="text-slate-500">No firmware information available</div>
          )}
        </CardContent>
      </Card>

      {/* OTA Upload */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <FileUp size={14} /> OTA Firmware Update
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          {/* File picker */}
          <div
            onClick={() => fileInputRef.current?.click()}
            className="flex cursor-pointer flex-col items-center gap-3 rounded-lg border-2 border-dashed border-slate-600 p-8 transition-colors hover:border-indigo-500 hover:bg-slate-800/30"
          >
            <Upload size={32} className="text-slate-400" />
            {selectedFile ? (
              <div className="text-center">
                <div className="font-medium text-white">{selectedFile.name}</div>
                <div className="text-sm text-slate-400">{formatBytes(selectedFile.size)}</div>
              </div>
            ) : (
              <div className="text-center text-sm text-slate-400">
                Click to select .bin firmware file
              </div>
            )}
            <input
              ref={fileInputRef}
              type="file"
              accept=".bin"
              className="hidden"
              onChange={(e) => {
                const f = e.target.files?.[0]
                if (f) setSelectedFile(f)
                setUploadStatus('idle')
                setUploadError('')
              }}
            />
          </div>

          {/* Progress */}
          {uploadStatus !== 'idle' && (
            <div className="space-y-2">
              <div className="flex items-center justify-between text-sm">
                <Badge variant={stateVariant}>{stateText}</Badge>
                <span className="font-mono text-slate-400">{uploadProgress}%</span>
              </div>
              <Progress
                value={uploadProgress}
                indicatorClassName={
                  uploadStatus === 'complete'
                    ? 'bg-emerald-500'
                    : uploadStatus === 'failed'
                    ? 'bg-red-500'
                    : 'bg-indigo-500'
                }
              />
              {uploadError && (
                <div className="flex items-center gap-2 rounded bg-red-900/30 p-2 text-sm text-red-300">
                  <AlertTriangle size={14} />
                  {uploadError}
                </div>
              )}
              {uploadStatus === 'complete' && (
                <div className="flex items-center gap-2 text-sm text-emerald-300">
                  <CheckCircle size={14} />
                  Upload complete — reboot to apply
                </div>
              )}
            </div>
          )}

          {/* Actions */}
          <div className="flex gap-2">
            <Button
              className="flex-1"
              onClick={startUpload}
              disabled={!selectedFile || isUploading}
            >
              <Upload size={14} />
              {isUploading ? 'Uploading…' : 'Upload Firmware'}
            </Button>
            {isUploading && (
              <Button variant="destructive" onClick={abortUpload}>
                Abort
              </Button>
            )}
          </div>
        </CardContent>
      </Card>

      {/* Device control */}
      <Card>
        <CardHeader><CardTitle>Device Control</CardTitle></CardHeader>
        <CardContent className="space-y-4">
          {/* Reboot */}
          <div className="flex items-center justify-between rounded-lg bg-slate-800/50 p-4">
            <div>
              <div className="font-medium text-white">Reboot</div>
              <div className="text-xs text-slate-400">Restart the RX72N microcontroller</div>
              <div className="mt-1 flex items-center gap-2 text-xs">
                <span className="text-slate-500">Delay:</span>
                <select
                  className="rounded border border-slate-600 bg-slate-700 px-2 py-0.5 text-xs text-white"
                  value={rebootDelay}
                  onChange={(e) => setRebootDelay(parseInt(e.target.value, 10))}
                >
                  {[1, 3, 5, 10].map((s) => (
                    <option key={s} value={s}>{s}s</option>
                  ))}
                </select>
              </div>
            </div>
            <Button
              variant="warning"
              onClick={handleReboot}
              disabled={actionState !== 'idle'}
            >
              <Power size={14} />
              {actionState === 'rebooting' ? 'Rebooting…' : 'Reboot'}
            </Button>
          </div>

          {/* Rollback */}
          <div className="flex items-center justify-between rounded-lg bg-slate-800/50 p-4">
            <div>
              <div className="font-medium text-white">Rollback</div>
              <div className="text-xs text-slate-400">Switch to the previous firmware bank</div>
            </div>
            <Button
              variant="outline"
              onClick={handleRollback}
              disabled={actionState !== 'idle'}
            >
              <RotateCcw size={14} />
              {actionState === 'rolling-back' ? 'Rolling back…' : 'Rollback'}
            </Button>
          </div>

          {/* Mark valid */}
          <div className="flex items-center justify-between rounded-lg bg-slate-800/50 p-4">
            <div>
              <div className="font-medium text-white">Mark as Valid</div>
              <div className="text-xs text-slate-400">Confirm current firmware is stable (OTA trial)</div>
            </div>
            <Button
              variant="success"
              onClick={handleMarkValid}
              disabled={actionState !== 'idle'}
            >
              <CheckCircle size={14} />
              {actionState === 'marking' ? 'Marking…' : 'Mark Valid'}
            </Button>
          </div>
        </CardContent>
      </Card>
    </div>
  )
}
