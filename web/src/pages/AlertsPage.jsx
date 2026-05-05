import { useState, useEffect, useMemo } from 'react'
import { api } from '../api'
import { relativeTime, formatDateTime } from '../utils/format'

const SEVERITY = {
  CRITICAL: { label: 'Critical', color: 'text-red-400 bg-red-500/10 border-red-500/20' },
  WARNING:  { label: 'Warning',  color: 'text-orange-400 bg-orange-500/10 border-orange-500/20' },
}

export default function AlertsPage() {
  const [alerts, setAlerts] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState('')
  const [filter, setFilter] = useState('all')

  async function load() {
    setLoading(true)
    setError('')
    try {
      console.log('[Alerts] Loading from backend...')
      const data = await api.getAlerts()
      console.log('[Alerts] Loaded:', data.length, 'alerts')
      setAlerts(data)
    } catch (err) {
      console.error('[Alerts] Load failed:', err.message)
      setError(err.message)
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => { load() }, [])

  async function markRead(alert) {
    console.log('[Alerts] Marking read: id=', alert.id)
    setAlerts(prev => prev.map(a => a.id === alert.id ? { ...a, status: 'READ' } : a))
    try {
      await api.markAlertRead(alert.id)
    } catch (err) {
      console.error('[Alerts] Mark read failed:', err.message)
    }
  }

  async function markAllRead() {
    console.log('[Alerts] Marking all read')
    setAlerts(prev => prev.map(a => ({ ...a, status: 'READ' })))
    try {
      await api.markAllAlertsRead()
    } catch (err) {
      console.error('[Alerts] Mark all read failed:', err.message)
    }
  }

  const filtered = useMemo(() => alerts.filter(a => {
    if (filter === 'critical') return a.severity === 'CRITICAL'
    if (filter === 'warning')  return a.severity === 'WARNING'
    if (filter === 'unread')   return a.status === 'UNREAD'
    return true
  }), [alerts, filter])

  const unreadCount = alerts.filter(a => a.status === 'UNREAD').length

  return (
    <div className="p-8">
      <div className="flex items-center justify-between mb-6">
        <div>
          <h1 className="text-xl font-bold text-white">Alerts</h1>
          <p className="text-sm text-gray-500 mt-0.5">
            {unreadCount > 0 ? `${unreadCount} unread alert${unreadCount === 1 ? '' : 's'}` : 'No unread alerts'}
          </p>
        </div>
        <div className="flex gap-3">
          <button onClick={load} className="bg-gray-800 hover:bg-gray-700 border border-gray-700 text-gray-300 px-4 py-2 rounded-lg text-sm transition">
            Refresh
          </button>
          {unreadCount > 0 && (
            <button onClick={markAllRead} className="text-sm text-gray-400 hover:text-gray-200 border border-gray-700 px-4 py-2 rounded-lg transition">
              Mark All Read
            </button>
          )}
        </div>
      </div>

      {/* Filters */}
      <div className="flex gap-2 mb-5">
        {[['all', 'All'], ['critical', 'Critical'], ['warning', 'Warning'], ['unread', 'Unread']].map(([val, label]) => (
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
              <th className="text-left px-5 py-3 font-medium">Alert</th>
              <th className="text-left px-5 py-3 font-medium">Detail</th>
              <th className="text-left px-5 py-3 font-medium">Severity</th>
              <th className="text-left px-5 py-3 font-medium">Time</th>
              <th className="px-5 py-3" />
            </tr>
          </thead>
          <tbody>
            {loading && (
              <tr><td colSpan={5} className="text-center text-gray-500 py-12">Loading...</td></tr>
            )}
            {!loading && filtered.length === 0 && (
              <tr><td colSpan={5} className="text-center text-gray-500 py-12">No alerts</td></tr>
            )}
            {filtered.map(alert => {
              const sev = SEVERITY[alert.severity] || SEVERITY.WARNING
              const isUnread = alert.status === 'UNREAD'
              return (
                <tr key={alert.id} className={`border-b border-gray-800/60 hover:bg-gray-800/30 transition ${isUnread ? 'bg-gray-800/20' : ''}`}>
                  <td className="px-5 py-3.5">
                    <div className="flex items-center gap-2">
                      {isUnread && <span className="w-2 h-2 rounded-full bg-blue-400 flex-shrink-0" />}
                      <span className="text-gray-200 font-medium">{alert.title}</span>
                    </div>
                  </td>
                  <td className="px-5 py-3.5 text-gray-400 max-w-xs truncate">{alert.detail}</td>
                  <td className="px-5 py-3.5">
                    <span className={`px-2 py-0.5 rounded-md text-xs font-medium border ${sev.color}`}>
                      {sev.label}
                    </span>
                  </td>
                  <td className="px-5 py-3.5 text-gray-500 text-xs">
                    <div>{relativeTime(alert.created_at)}</div>
                    <div className="text-gray-600">{formatDateTime(alert.created_at)}</div>
                  </td>
                  <td className="px-5 py-3.5 text-right">
                    {isUnread && (
                      <button
                        onClick={() => markRead(alert)}
                        className="text-xs text-gray-500 hover:text-blue-400 transition"
                      >
                        Mark Read
                      </button>
                    )}
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
