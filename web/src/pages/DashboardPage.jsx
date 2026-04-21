import { useState, useEffect, useCallback, useRef } from 'react'
import { Link } from 'react-router-dom'
import { api } from '../api'
import { useSession } from '../context/SessionContext'

const REFRESH_INTERVAL = 30_000

function StatCard({ title, value, sub, accent }) {
  const accents = {
    green: 'border-l-green-500',
    blue: 'border-l-blue-500',
    orange: 'border-l-orange-500',
    red: 'border-l-red-500',
    gray: 'border-l-gray-600',
  }
  return (
    <div className={`bg-gray-900 border border-gray-800 border-l-4 ${accents[accent] || accents.gray} rounded-xl p-5`}>
      <p className="text-xs font-medium text-gray-500 uppercase tracking-wide mb-2">{title}</p>
      <p className="text-2xl font-bold text-white">{value}</p>
      {sub && <p className="text-xs text-gray-500 mt-1">{sub}</p>}
    </div>
  )
}

function WeeklyChart({ allLogs }) {
  const days = Array.from({ length: 7 }, (_, i) => {
    const d = new Date()
    d.setDate(d.getDate() - (6 - i))
    d.setHours(0, 0, 0, 0)
    return d
  })

  const data = days.map(day => {
    const next = new Date(day)
    next.setDate(next.getDate() + 1)
    const dayLogs = allLogs.filter(l => {
      const t = new Date(l.time)
      return t >= day && t < next
    })
    return {
      label: day.toLocaleDateString('tr-TR', { weekday: 'short' }),
      success: dayLogs.filter(l => l.success).length,
      fail: dayLogs.filter(l => !l.success).length,
    }
  })

  const max = Math.max(...data.map(d => d.success + d.fail), 1)

  return (
    <div className="bg-gray-900 border border-gray-800 rounded-xl p-5">
      <div className="flex items-center justify-between mb-5">
        <h2 className="text-sm font-semibold text-white">Son 7 Gün</h2>
        <div className="flex items-center gap-4 text-xs text-gray-500">
          <span className="flex items-center gap-1.5">
            <span className="w-2.5 h-2.5 rounded-sm bg-green-500 inline-block" /> Başarılı
          </span>
          <span className="flex items-center gap-1.5">
            <span className="w-2.5 h-2.5 rounded-sm bg-red-500 inline-block" /> Başarısız
          </span>
        </div>
      </div>
      <div className="flex items-end gap-2 h-32">
        {data.map((d, i) => {
          const total = d.success + d.fail
          const successH = total ? (d.success / max) * 100 : 0
          const failH = total ? (d.fail / max) * 100 : 0
          return (
            <div key={i} className="flex-1 flex flex-col items-center gap-1">
              <div className="w-full flex flex-col justify-end gap-0.5" style={{ height: '96px' }}>
                {failH > 0 && (
                  <div
                    className="w-full bg-red-500/70 rounded-t-sm transition-all duration-500"
                    style={{ height: `${failH}%` }}
                    title={`${d.fail} başarısız`}
                  />
                )}
                {successH > 0 && (
                  <div
                    className="w-full bg-green-500/70 rounded-t-sm transition-all duration-500"
                    style={{ height: `${successH}%`, order: failH > 0 ? 1 : 0 }}
                    title={`${d.success} başarılı`}
                  />
                )}
                {total === 0 && (
                  <div className="w-full bg-gray-800 rounded-sm" style={{ height: '4px' }} />
                )}
              </div>
              <span className="text-xs text-gray-600">{d.label}</span>
            </div>
          )
        })}
      </div>
    </div>
  )
}

export default function DashboardPage() {
  const { session } = useSession()
  const [status, setStatus] = useState(null)
  const [recentLogs, setRecentLogs] = useState([])
  const [allLogs, setAllLogs] = useState([])
  const [loading, setLoading] = useState(true)
  const [lastRefresh, setLastRefresh] = useState(null)
  const [showWarning, setShowWarning] = useState(false)
  const timerRef = useRef(null)

  const load = useCallback(async (silent = false) => {
    if (!silent) setLoading(true)
    try {
      const [statusData, logsData] = await Promise.all([api.getStatus(), api.getLogs()])
      setStatus(statusData)
      setRecentLogs(logsData.slice(0, 5))
      setAllLogs(logsData)
      setLastRefresh(new Date())
    } catch {
      setStatus(null)
    } finally {
      if (!silent) setLoading(false)
    }
  }, [])

  // İlk yükleme
  useEffect(() => { load() }, [load])

  // 30 saniyede bir sessiz yenileme
  useEffect(() => {
    timerRef.current = setInterval(() => load(true), REFRESH_INTERVAL)
    return () => clearInterval(timerRef.current)
  }, [load])

  const isOnline = status !== null
  const isUnlocked = status?.lock_status === 'Unlocked'

  return (
    <div className="p-8">
      {/* Header */}
      <div className="flex items-center justify-between mb-8">
        <div>
          <h1 className="text-xl font-bold text-white">Dashboard</h1>
          <p className="text-sm text-gray-500 mt-0.5">
            Hoşgeldin, {session?.name}
            {' · '}
            <span className={isOnline ? 'text-green-400' : 'text-red-400'}>
              {loading ? '...' : isOnline ? 'Sistem çevrimiçi' : 'Bağlantı yok'}
            </span>
            {lastRefresh && !loading && (
              <span className="text-gray-600 ml-2">
                · {lastRefresh.toLocaleTimeString('tr-TR', { hour: '2-digit', minute: '2-digit', second: '2-digit' })} güncellendi
              </span>
            )}
          </p>
        </div>
        <button
          onClick={() => load()}
          className="bg-gray-800 hover:bg-gray-700 border border-gray-700 text-gray-300 px-4 py-2 rounded-lg text-sm transition"
        >
          ↺ Yenile
        </button>
      </div>

      {/* Stats */}
      <div className="grid grid-cols-4 gap-4 mb-6">
        <StatCard
          title="Cihaz Durumu"
          value={isOnline ? 'Çevrimiçi' : 'Çevrimdışı'}
          accent={isOnline ? 'green' : 'gray'}
        />
        <StatCard
          title="Kapı"
          value={isUnlocked ? 'Açık' : 'Kilitli'}
          sub={isUnlocked ? 'Erişime açık' : 'Güvende'}
          accent={isUnlocked ? 'green' : 'blue'}
        />
        <StatCard
          title="Son Erişim"
          value={status?.last_event || '—'}
          accent="orange"
        />
        <StatCard
          title="Başarısız Deneme"
          value={String(status?.fail_count ?? '—')}
          sub="Son erişimde"
          accent={(status?.fail_count ?? 0) > 0 ? 'red' : 'gray'}
        />
      </div>

      {/* Chart + Quick Actions */}
      <div className="grid grid-cols-3 gap-6 mb-6">
        <div className="col-span-2">
          <WeeklyChart allLogs={allLogs} />
        </div>
        <div className="bg-gray-900 border border-gray-800 rounded-xl p-5">
          <h2 className="text-sm font-semibold text-white mb-4">Hızlı İşlemler</h2>
          <div className="space-y-2">
            <Link to="/users" className="flex items-center gap-3 px-4 py-3 bg-gray-800 hover:bg-gray-700 rounded-lg text-sm text-gray-300 transition">
              <span>👥</span> Kullanıcıları Yönet
            </Link>
            <Link to="/logs" className="flex items-center gap-3 px-4 py-3 bg-gray-800 hover:bg-gray-700 rounded-lg text-sm text-gray-300 transition">
              <span>📋</span> Tüm Kayıtlar
            </Link>
            <Link to="/alerts" className="flex items-center gap-3 px-4 py-3 bg-gray-800 hover:bg-gray-700 rounded-lg text-sm text-gray-300 transition">
              <span>🔔</span> Uyarılar
            </Link>
            <button
              onClick={() => setShowWarning(true)}
              className="w-full flex items-center gap-3 px-4 py-3 bg-red-500/10 hover:bg-red-500/20 border border-red-500/20 rounded-lg text-sm text-red-400 transition"
            >
              <span>⚠️</span> Güvenlik Uyarısı Gönder
            </button>
          </div>
        </div>
      </div>

      {/* Recent Logs */}
      <div className="bg-gray-900 border border-gray-800 rounded-xl overflow-hidden">
        <div className="flex items-center justify-between px-5 py-4 border-b border-gray-800">
          <h2 className="text-sm font-semibold text-white">Son Erişimler</h2>
          <Link to="/logs" className="text-xs text-blue-400 hover:text-blue-300 transition">
            Tümünü gör →
          </Link>
        </div>
        <table className="w-full text-sm">
          <tbody>
            {loading && (
              <tr><td className="text-center text-gray-500 py-8">Yükleniyor...</td></tr>
            )}
            {!loading && recentLogs.length === 0 && (
              <tr><td className="text-center text-gray-500 py-8">Kayıt yok</td></tr>
            )}
            {recentLogs.map(log => (
              <tr key={log.id} className="border-b border-gray-800/60 hover:bg-gray-800/40 transition">
                <td className="px-5 py-3">
                  <span className={`inline-block w-2 h-2 rounded-full mr-2 ${log.success ? 'bg-green-400' : 'bg-red-400'}`} />
                  <span className="text-gray-300">{log.user?.name || '—'}</span>
                </td>
                <td className="px-5 py-3 text-gray-500 text-xs">{log.status}</td>
                <td className="px-5 py-3 text-gray-500 text-xs text-right">
                  {new Date(log.time).toLocaleString('tr-TR')}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* Warning Modal */}
      {showWarning && (
        <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50" onClick={() => setShowWarning(false)}>
          <div className="bg-gray-900 border border-gray-700 rounded-2xl w-full max-w-sm overflow-hidden shadow-2xl" onClick={e => e.stopPropagation()}>
            <div className="p-6 text-center">
              <div className="text-4xl mb-3">⚠️</div>
              <h2 className="text-base font-bold text-white">Güvenlik Uyarısı</h2>
              <p className="text-gray-400 text-sm mt-1">İzinsiz / Hatalı giriş tespit edildi</p>
            </div>
            <div className="border-t border-gray-800 flex">
              <button onClick={() => setShowWarning(false)} className="flex-1 py-3.5 text-gray-400 text-sm hover:bg-gray-800 transition border-r border-gray-800">
                Yoksay
              </button>
              <Link to="/alerts" onClick={() => setShowWarning(false)} className="flex-1 py-3.5 text-red-400 text-sm font-medium hover:bg-gray-800 transition text-center">
                Uyarılara Git
              </Link>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
