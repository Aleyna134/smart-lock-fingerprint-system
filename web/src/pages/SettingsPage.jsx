import { useSession } from '../context/SessionContext'
import { useNavigate } from 'react-router-dom'

export default function SettingsPage() {
  const { session, logout } = useSession()
  const navigate = useNavigate()

  function handleLogout() {
    logout()
    navigate('/login', { replace: true })
  }

  return (
    <div className="p-8 max-w-xl">
      <div className="mb-6">
        <h1 className="text-xl font-bold text-white">Ayarlar</h1>
      </div>

      {/* Account */}
      <div className="bg-gray-900 border border-gray-800 rounded-xl overflow-hidden mb-4">
        <div className="px-5 py-3.5 border-b border-gray-800">
          <p className="text-sm font-semibold text-white">Hesap Bilgileri</p>
        </div>
        <div className="px-5 py-4 space-y-3 text-sm">
          <div className="flex justify-between py-2 border-b border-gray-800">
            <span className="text-gray-500">Ad</span>
            <span className="text-gray-200">{session?.name}</span>
          </div>
          <div className="flex justify-between py-2 border-b border-gray-800">
            <span className="text-gray-500">Rol</span>
            <span className={`px-2 py-0.5 rounded-md text-xs font-medium ${
              session?.role === 'ADMIN'
                ? 'bg-purple-500/15 text-purple-400 border border-purple-500/20'
                : 'bg-blue-500/15 text-blue-400 border border-blue-500/20'
            }`}>
              {session?.role}
            </span>
          </div>
          <div className="flex justify-between py-2">
            <span className="text-gray-500">Kullanıcı ID</span>
            <span className="text-gray-200">#{session?.id}</span>
          </div>
        </div>
      </div>

      {/* Logout */}
      <div className="bg-gray-900 border border-gray-800 rounded-xl overflow-hidden">
        <div className="px-5 py-3.5 border-b border-gray-800">
          <p className="text-sm font-semibold text-white">Oturum</p>
        </div>
        <div className="px-5 py-4">
          <button
            onClick={handleLogout}
            className="bg-red-600/10 hover:bg-red-600/20 border border-red-500/30 text-red-400 px-5 py-2.5 rounded-xl text-sm font-medium transition"
          >
            Çıkış Yap
          </button>
        </div>
      </div>
    </div>
  )
}
