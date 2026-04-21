import { useState, useEffect, useMemo } from 'react'
import { api } from '../api'

function relativeTime(dateStr) {
  const diff = (Date.now() - new Date(dateStr)) / 1000
  if (diff < 60) return 'Az önce'
  if (diff < 3600) return `${Math.floor(diff / 60)} dk önce`
  if (diff < 86400) return `${Math.floor(diff / 3600)} sa önce`
  return `${Math.floor(diff / 86400)} gün önce`
}

export default function AccessLogsPage() {
  const [logs, setLogs] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState('')
  const [search, setSearch] = useState('')
  const [filter, setFilter] = useState('all')
  const [selectedLog, setSelectedLog] = useState(null)

  async function load() {
    setLoading(true)
    setError('')
    try {
      setLogs(await api.getLogs())
    } catch (err) {
      setError(err.message)
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => { load() }, [])

  const filtered = useMemo(() => logs.filter(log => {
    const name = log.user?.name?.toLowerCase() || ''
    const matchSearch = !search || name.includes(search.toLowerCase()) || log.status.toLowerCase().includes(search.toLowerCase())
    const matchFilter = filter === 'all' || (filter === 'success' && log.success) || (filter === 'fail' && !log.success)
    return matchSearch && matchFilter
  }), [logs, search, filter])

  return (
    <div className="p-8">
      {/* Header */}
      <div className="flex items-center justify-between mb-6">
        <div>
          <h1 className="text-xl font-bold text-white">Erişim Kayıtları</h1>
          <p className="text-sm text-gray-500 mt-0.5">{logs.length} kayıt</p>
        </div>
        <button
          onClick={load}
          className="bg-gray-800 hover:bg-gray-700 border border-gray-700 text-gray-300 px-4 py-2 rounded-lg text-sm transition flex items-center gap-2"
        >
          ↺ Yenile
        </button>
      </div>

      {/* Filters */}
      <div className="flex gap-3 mb-5">
        <input
          type="text"
          value={search}
          onChange={e => setSearch(e.target.value)}
          placeholder="Kullanıcı veya durum ara..."
          className="bg-gray-900 border border-gray-700 rounded-lg px-3 py-2 text-sm text-white placeholder-gray-500 focus:outline-none focus:border-blue-500 transition w-72"
        />
        <div className="flex rounded-lg border border-gray-700 overflow-hidden">
          {[['all', 'Tümü'], ['success', 'Başarılı'], ['fail', 'Başarısız']].map(([val, label]) => (
            <button
              key={val}
              onClick={() => setFilter(val)}
              className={`px-4 py-2 text-sm transition ${
                filter === val
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-900 text-gray-400 hover:bg-gray-800'
              }`}
            >
              {label}
            </button>
          ))}
        </div>
      </div>

      {error && (
        <div className="bg-red-500/10 border border-red-500/30 rounded-lg px-4 py-3 text-sm text-red-400 mb-4">
          {error}
        </div>
      )}

      {/* Table */}
      <div className="bg-gray-900 border border-gray-800 rounded-xl overflow-hidden">
        <table className="w-full text-sm">
          <thead>
            <tr className="border-b border-gray-800 text-xs text-gray-500 uppercase tracking-wide">
              <th className="text-left px-5 py-3 font-medium">Kullanıcı</th>
              <th className="text-left px-5 py-3 font-medium">Durum</th>
              <th className="text-left px-5 py-3 font-medium">Sonuç</th>
              <th className="text-left px-5 py-3 font-medium">Başarısız Deneme</th>
              <th className="text-left px-5 py-3 font-medium">Zaman</th>
            </tr>
          </thead>
          <tbody>
            {loading && (
              <tr>
                <td colSpan={5} className="text-center text-gray-500 py-12">Yükleniyor...</td>
              </tr>
            )}
            {!loading && filtered.length === 0 && (
              <tr>
                <td colSpan={5} className="text-center text-gray-500 py-12">
                  {search || filter !== 'all' ? 'Sonuç bulunamadı' : 'Kayıt yok'}
                </td>
              </tr>
            )}
            {filtered.map(log => (
              <tr
                key={log.id}
                onClick={() => setSelectedLog(log)}
                className="border-b border-gray-800/60 hover:bg-gray-800/50 cursor-pointer transition"
              >
                <td className="px-5 py-3.5">
                  <div className="font-medium text-white">{log.user?.name || '—'}</div>
                  <div className="text-xs text-gray-500">{log.user?.email || ''}</div>
                </td>
                <td className="px-5 py-3.5 text-gray-300">{log.status}</td>
                <td className="px-5 py-3.5">
                  <span className={`inline-flex items-center gap-1.5 px-2.5 py-1 rounded-md text-xs font-medium ${
                    log.success
                      ? 'bg-green-500/10 text-green-400 border border-green-500/20'
                      : 'bg-red-500/10 text-red-400 border border-red-500/20'
                  }`}>
                    {log.success ? '✓ Başarılı' : '✕ Başarısız'}
                  </span>
                </td>
                <td className="px-5 py-3.5 text-gray-400">
                  {log.fail_count > 0
                    ? <span className="text-orange-400 font-medium">{log.fail_count}</span>
                    : <span className="text-gray-600">0</span>
                  }
                </td>
                <td className="px-5 py-3.5 text-gray-400">
                  <div>{relativeTime(log.time)}</div>
                  <div className="text-xs text-gray-600">{new Date(log.time).toLocaleString('tr-TR')}</div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* Detail Modal */}
      {selectedLog && (
        <div
          className="fixed inset-0 bg-black/60 flex items-center justify-center z-50 px-4"
          onClick={() => setSelectedLog(null)}
        >
          <div
            className="bg-gray-900 border border-gray-700 rounded-2xl w-full max-w-md p-6 shadow-2xl"
            onClick={e => e.stopPropagation()}
          >
            <div className="flex items-start justify-between mb-5">
              <h2 className="text-base font-bold text-white">Erişim Detayı</h2>
              <button onClick={() => setSelectedLog(null)} className="text-gray-500 hover:text-gray-300 text-lg leading-none">✕</button>
            </div>
            <div className="space-y-3 text-sm">
              <Row label="Kullanıcı" value={selectedLog.user?.name || '—'} />
              <Row label="E-posta" value={selectedLog.user?.email || '—'} />
              <Row label="Rol" value={selectedLog.user?.role || '—'} />
              <Row label="Durum" value={selectedLog.status} />
              <Row label="Sonuç" value={selectedLog.success ? 'Başarılı' : 'Başarısız'} />
              <Row label="Başarısız Deneme" value={String(selectedLog.fail_count)} />
              <Row label="Zaman" value={new Date(selectedLog.time).toLocaleString('tr-TR')} />
            </div>
          </div>
        </div>
      )}
    </div>
  )
}

function Row({ label, value }) {
  return (
    <div className="flex justify-between py-2 border-b border-gray-800">
      <span className="text-gray-500">{label}</span>
      <span className="text-gray-200 font-medium">{value}</span>
    </div>
  )
}
