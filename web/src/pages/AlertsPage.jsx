import { useState, useEffect, useMemo } from 'react'
import { api } from '../api'
import { formatDateTime, formatStatus, relativeTime } from '../utils/format'

const SEVERITY = {
  high: { label: 'High', color: 'text-red-400 bg-red-500/10 border-red-500/20' },
  medium: { label: 'Medium', color: 'text-orange-400 bg-orange-500/10 border-orange-500/20' },
  low: { label: 'Low', color: 'text-yellow-400 bg-yellow-500/10 border-yellow-500/20' },
}

function getSeverity(log) {
  if (log.fail_count >= 3) return 'high'
  if (log.fail_count === 2) return 'medium'
  return 'low'
}

export default function AlertsPage() {
  const [logs, setLogs] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState('')
  const [dismissed, setDismissed] = useState(() => {
    const stored = localStorage.getItem('dismissed_alerts')
    return stored ? JSON.parse(stored) : []
  })
  const [filter, setFilter] = useState('all')

  async function load() {
    setLoading(true)
    setError('')
    try {
      const data = await api.getLogs()
      setLogs(data.filter(l => !l.success))
    } catch (err) {
      setError(err.message)
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => { load() }, [])

  function dismiss(id) {
    const next = [...dismissed, id]
    setDismissed(next)
    localStorage.setItem('dismissed_alerts', JSON.stringify(next))
  }

  function dismissAll() {
    const next = [...dismissed, ...active.map(l => l.id)]
    setDismissed(next)
    localStorage.setItem('dismissed_alerts', JSON.stringify(next))
  }

  const active = useMemo(() =>
    logs.filter(l => !dismissed.includes(l.id)),
    [logs, dismissed]
  )

  const filtered = useMemo(() => active.filter(l => {
    if (filter === 'high') return getSeverity(l) === 'high'
    if (filter === 'medium') return getSeverity(l) === 'medium'
    if (filter === 'low') return getSeverity(l) === 'low'
    return true
  }), [active, filter])

  return (
    <div className="p-8">
      <div className="flex items-center justify-between mb-6">
        <div>
          <h1 className="text-xl font-bold text-white">Alerts</h1>
          <p className="text-sm text-gray-500 mt-0.5">
            {active.length > 0 ? `${active.length} active alert${active.length === 1 ? '' : 's'}` : 'No active alerts'}
          </p>
        </div>
        <div className="flex gap-3">
          <button onClick={load} className="bg-gray-800 hover:bg-gray-700 border border-gray-700 text-gray-300 px-4 py-2 rounded-lg text-sm transition">
            Refresh
          </button>
          {active.length > 0 && (
            <button onClick={dismissAll} className="text-sm text-gray-400 hover:text-gray-200 border border-gray-700 px-4 py-2 rounded-lg transition">
              Dismiss All
            </button>
          )}
        </div>
      </div>

      {/* Filters */}
      <div className="flex gap-2 mb-5">
        {[['all', 'All'], ['high', 'High'], ['medium', 'Medium'], ['low', 'Low']].map(([val, label]) => (
          <button
            key={val}
            onClick={() => setFilter(val)}
            className={`px-3 py-1.5 rounded-lg text-xs font-medium transition border ${
              filter === val
                ? 'bg-blue-600 text-white border-blue-600'
                : 'bg-gray-900 text-gray-400 border-gray-700 hover:border-gray-500'
            }`}
          >
            {label}
          </button>
        ))}
      </div>

      {error && (
        <div className="bg-red-500/10 border border-red-500/30 rounded-lg px-4 py-3 text-sm text-red-400 mb-4">
          {error}
        </div>
      )}

      <div className="bg-gray-900 border border-gray-800 rounded-xl overflow-hidden">
        <table className="w-full text-sm">
          <thead>
            <tr className="text-xs text-gray-500 uppercase tracking-wide border-b border-gray-800">
              <th className="text-left px-5 py-3 font-medium">Status</th>
              <th className="text-left px-5 py-3 font-medium">User</th>
              <th className="text-left px-5 py-3 font-medium">Severity</th>
              <th className="text-left px-5 py-3 font-medium">Failed Attempts</th>
              <th className="text-left px-5 py-3 font-medium">Time</th>
              <th className="px-5 py-3" />
            </tr>
          </thead>
          <tbody>
            {loading && (
              <tr><td colSpan={6} className="text-center text-gray-500 py-12">Loading...</td></tr>
            )}
            {!loading && filtered.length === 0 && (
              <tr><td colSpan={6} className="text-center text-gray-500 py-12">No alerts</td></tr>
            )}
            {filtered.map(log => {
              const sev = getSeverity(log)
              return (
                <tr key={log.id} className="border-b border-gray-800/60 hover:bg-gray-800/30 transition">
                  <td className="px-5 py-3.5 text-gray-300 font-medium">{formatStatus(log.status)}</td>
                  <td className="px-5 py-3.5 text-gray-400">{log.user?.name || 'Unknown'}</td>
                  <td className="px-5 py-3.5">
                    <span className={`px-2 py-0.5 rounded-md text-xs font-medium border ${SEVERITY[sev].color}`}>
                      {SEVERITY[sev].label}
                    </span>
                  </td>
                  <td className="px-5 py-3.5">
                    <span className="text-orange-400 font-medium">{log.fail_count}</span>
                  </td>
                  <td className="px-5 py-3.5 text-gray-500 text-xs">
                    <div>{relativeTime(log.time)}</div>
                    <div className="text-gray-600">{formatDateTime(log.time)}</div>
                  </td>
                  <td className="px-5 py-3.5 text-right">
                    <button
                      onClick={() => dismiss(log.id)}
                      className="text-xs text-gray-500 hover:text-red-400 transition"
                    >
                      Dismiss
                    </button>
                  </td>
                </tr>
              )
            })}
          </tbody>
        </table>
      </div>
    </div>
  )
}
