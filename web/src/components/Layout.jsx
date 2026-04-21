import { useEffect, useState } from 'react'
import { Outlet, NavLink, useNavigate } from 'react-router-dom'
import { useSession } from '../context/SessionContext'
import { api } from '../api'

const navItems = [
  { to: '/dashboard', label: 'Dashboard', icon: '⊞' },
  { to: '/logs', label: 'Erişim Kayıtları', icon: '📋' },
  { to: '/users', label: 'Kullanıcılar', icon: '👥' },
  { to: '/alerts', label: 'Uyarılar', icon: '🔔' },
  { to: '/settings', label: 'Ayarlar', icon: '⚙️' },
]

const STORAGE_KEY = 'smartlock_last_seen'

export default function Layout() {
  const { session } = useSession()
  const [failedSince, setFailedSince] = useState([])
  const [dismissed, setDismissed] = useState(false)

  useEffect(() => {
    const lastSeen = localStorage.getItem(STORAGE_KEY)
    const lastSeenDate = lastSeen ? new Date(lastSeen) : new Date(0)

    api.getLogs().then(logs => {
      const newFailed = logs.filter(log =>
        !log.success && new Date(log.time) > lastSeenDate
      )
      if (newFailed.length > 0) setFailedSince(newFailed)
    }).catch(() => {})

    // Bu ziyareti kaydet
    localStorage.setItem(STORAGE_KEY, new Date().toISOString())
  }, [])

  const showBanner = failedSince.length > 0 && !dismissed

  return (
    <div className="flex h-screen bg-gray-950 text-gray-100">
      {/* Sidebar */}
      <aside className="w-60 shrink-0 flex flex-col bg-gray-900 border-r border-gray-800">
        {/* Logo */}
        <div className="px-6 py-5 border-b border-gray-800">
          <div className="flex items-center gap-3">
            <div className="w-9 h-9 bg-blue-600 rounded-xl flex items-center justify-center text-lg">
              🔒
            </div>
            <div>
              <p className="text-sm font-bold text-white leading-tight">Smart Lock</p>
              <p className="text-xs text-gray-400">Erişim Kontrol</p>
            </div>
          </div>
        </div>

        {/* Nav */}
        <nav className="flex-1 px-3 py-4 space-y-1">
          {navItems.map(({ to, label, icon }) => (
            <NavLink
              key={to}
              to={to}
              className={({ isActive }) =>
                `flex items-center gap-3 px-3 py-2.5 rounded-xl text-sm font-medium transition-colors ${
                  isActive
                    ? 'bg-blue-600/20 text-blue-400'
                    : 'text-gray-400 hover:bg-gray-800 hover:text-gray-100'
                }`
              }
            >
              <span className="text-base">{icon}</span>
              {label}
            </NavLink>
          ))}
        </nav>

        {/* User info */}
        <div className="px-4 py-4 border-t border-gray-800">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 bg-gray-700 rounded-full flex items-center justify-center text-sm font-bold text-gray-300">
              {session?.name?.charAt(0)?.toUpperCase() || 'U'}
            </div>
            <div className="min-w-0">
              <p className="text-sm font-medium text-gray-200 truncate">{session?.name}</p>
              <p className="text-xs text-gray-500 truncate">{session?.role}</p>
            </div>
          </div>
        </div>
      </aside>

      {/* Main content */}
      <div className="flex-1 flex flex-col overflow-hidden">
        <main className="flex-1 overflow-y-auto">
          <Outlet />
        </main>
      </div>

      {/* Blocking Security Alert Modal */}
      {showBanner && (
        <div className="fixed inset-0 z-50 bg-black/50 backdrop-blur-[2px] flex items-center justify-center px-4">
          <div className="bg-gray-900 border border-red-800 rounded-2xl w-full max-w-md shadow-2xl overflow-hidden">
            {/* Red top bar */}
            <div className="bg-red-600 px-6 py-4 flex items-center gap-3">
              <span className="text-2xl">⚠️</span>
              <div>
                <p className="text-white font-bold text-base">Güvenlik Uyarısı</p>
                <p className="text-red-200 text-xs mt-0.5">Bu uyarıyı onaylamadan devam edemezsiniz</p>
              </div>
            </div>

            {/* Content */}
            <div className="px-6 py-5">
              <p className="text-white text-sm font-semibold mb-1">
                Siz yokken <span className="text-red-400">{failedSince.length} başarısız giriş denemesi</span> tespit edildi.
              </p>
              <p className="text-gray-400 text-xs mb-5">
                Son deneme: {new Date(failedSince[0].time).toLocaleString('tr-TR')}
              </p>

              {/* Failed log list */}
              <div className="bg-gray-800 rounded-xl divide-y divide-gray-700 mb-5 max-h-48 overflow-y-auto">
                {failedSince.map(log => (
                  <div key={log.id} className="px-4 py-3 flex items-center justify-between">
                    <div>
                      <p className="text-sm text-red-300 font-medium">{log.status}</p>
                      <p className="text-xs text-gray-500 mt-0.5">
                        {new Date(log.time).toLocaleString('tr-TR')}
                      </p>
                    </div>
                    {log.fail_count > 0 && (
                      <span className="text-xs bg-red-500/15 text-red-400 border border-red-500/20 px-2 py-0.5 rounded-md">
                        {log.fail_count} deneme
                      </span>
                    )}
                  </div>
                ))}
              </div>

              {/* Actions */}
              <div className="flex gap-3">
                <NavLink
                  to="/logs"
                  onClick={() => setDismissed(true)}
                  className="flex-1 bg-red-600 hover:bg-red-500 text-white text-sm font-semibold py-3 rounded-xl transition text-center"
                >
                  Kayıtları İncele
                </NavLink>
                <button
                  onClick={() => setDismissed(true)}
                  className="flex-1 bg-gray-800 hover:bg-gray-700 text-gray-300 text-sm py-3 rounded-xl transition"
                >
                  Anladım, Kapat
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
