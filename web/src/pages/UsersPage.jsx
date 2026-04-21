import { useState, useEffect } from 'react'
import { api } from '../api'
import { useSession } from '../context/SessionContext'

export default function UsersPage() {
  const { session } = useSession()
  const [users, setUsers] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState('')
  const [showAdd, setShowAdd] = useState(false)
  const [selectedUser, setSelectedUser] = useState(null)
  const [deleteConfirm, setDeleteConfirm] = useState(null)

  async function load() {
    setLoading(true)
    setError('')
    try {
      setUsers(await api.getUsers())
    } catch (err) {
      setError(err.message)
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => { load() }, [])

  async function handleDelete(user) {
    try {
      await api.deleteUser(user.id)
      setDeleteConfirm(null)
      setSelectedUser(null)
      load()
    } catch (err) {
      setError(err.message)
    }
  }

  const admins = users.filter(u => u.role === 'ADMIN')
  const regulars = users.filter(u => u.role !== 'ADMIN')

  return (
    <div className="p-8">
      {/* Header */}
      <div className="flex items-center justify-between mb-6">
        <div>
          <h1 className="text-xl font-bold text-white">Kullanıcılar</h1>
          <p className="text-sm text-gray-500 mt-0.5">{users.length} kullanıcı</p>
        </div>
        <button
          onClick={() => setShowAdd(true)}
          className="bg-blue-600 hover:bg-blue-500 text-white px-4 py-2 rounded-lg text-sm font-medium transition flex items-center gap-2"
        >
          + Kullanıcı Ekle
        </button>
      </div>

      {error && (
        <div className="bg-red-500/10 border border-red-500/30 rounded-lg px-4 py-3 text-sm text-red-400 mb-4">
          {error}
        </div>
      )}

      {/* Admins */}
      <Section
        title="Yöneticiler"
        count={admins.length}
        users={admins}
        loading={loading}
        onSelect={setSelectedUser}
      />

      <div className="mb-6" />

      {/* Users */}
      <Section
        title="Kullanıcılar"
        count={regulars.length}
        users={regulars}
        loading={loading}
        onSelect={setSelectedUser}
      />

      {/* User Detail Drawer */}
      {selectedUser && (
        <div
          className="fixed inset-0 bg-black/60 flex items-center justify-center z-50 px-4"
          onClick={() => setSelectedUser(null)}
        >
          <div
            className="bg-gray-900 border border-gray-700 rounded-2xl w-full max-w-md shadow-2xl"
            onClick={e => e.stopPropagation()}
          >
            <div className="flex items-start justify-between px-6 pt-5 pb-4 border-b border-gray-800">
              <div className="flex items-center gap-3">
                <div className="w-10 h-10 bg-gray-700 rounded-full flex items-center justify-center text-sm font-bold text-gray-300">
                  {selectedUser.name.charAt(0).toUpperCase()}
                </div>
                <div>
                  <p className="font-semibold text-white">{selectedUser.name}</p>
                  <p className="text-xs text-gray-500">{selectedUser.email}</p>
                </div>
              </div>
              <button onClick={() => setSelectedUser(null)} className="text-gray-500 hover:text-gray-300">✕</button>
            </div>
            <div className="px-6 py-4 space-y-3 text-sm">
              <Row label="ID" value={`#${selectedUser.id}`} />
              <Row label="Rol" value={
                <span className={`px-2 py-0.5 rounded-md text-xs font-medium ${
                  selectedUser.role === 'ADMIN'
                    ? 'bg-purple-500/15 text-purple-400 border border-purple-500/20'
                    : 'bg-blue-500/15 text-blue-400 border border-blue-500/20'
                }`}>{selectedUser.role}</span>
              } />
              <Row label="Kayıt Tarihi" value={new Date(selectedUser.enrolled_at).toLocaleDateString('tr-TR', { day: 'numeric', month: 'long', year: 'numeric' })} />
            </div>
            {selectedUser.id !== session?.id && (
              <div className="px-6 pb-5">
                {deleteConfirm?.id === selectedUser.id ? (
                  <div className="bg-red-500/10 border border-red-500/20 rounded-xl p-4">
                    <p className="text-sm text-red-300 mb-3">
                      <strong>{selectedUser.name}</strong> silinecek. Bu işlem geri alınamaz.
                    </p>
                    <div className="flex gap-2">
                      <button
                        onClick={() => handleDelete(selectedUser)}
                        className="flex-1 bg-red-600 hover:bg-red-500 text-white py-2 rounded-lg text-sm font-medium transition"
                      >
                        Evet, Sil
                      </button>
                      <button
                        onClick={() => setDeleteConfirm(null)}
                        className="flex-1 bg-gray-800 hover:bg-gray-700 text-gray-300 py-2 rounded-lg text-sm transition"
                      >
                        İptal
                      </button>
                    </div>
                  </div>
                ) : (
                  <button
                    onClick={() => setDeleteConfirm(selectedUser)}
                    className="w-full border border-red-500/30 hover:bg-red-500/10 text-red-400 py-2.5 rounded-xl text-sm transition"
                  >
                    Kullanıcıyı Sil
                  </button>
                )}
              </div>
            )}
          </div>
        </div>
      )}

      {/* Add User Modal */}
      {showAdd && (
        <AddUserModal
          adminId={session?.id}
          onClose={() => setShowAdd(false)}
          onSuccess={() => { setShowAdd(false); load() }}
        />
      )}
    </div>
  )
}

function Section({ title, count, users, loading, onSelect }) {
  return (
    <div className="bg-gray-900 border border-gray-800 rounded-xl overflow-hidden">
      <div className="px-5 py-3.5 border-b border-gray-800 flex items-center gap-2">
        <span className="text-sm font-semibold text-white">{title}</span>
        <span className="text-xs text-gray-500 bg-gray-800 px-2 py-0.5 rounded-full">{count}</span>
      </div>
      <table className="w-full text-sm">
        <thead>
          <tr className="text-xs text-gray-500 uppercase tracking-wide border-b border-gray-800">
            <th className="text-left px-5 py-3 font-medium">Ad</th>
            <th className="text-left px-5 py-3 font-medium">E-posta</th>
            <th className="text-left px-5 py-3 font-medium">Rol</th>
            <th className="text-left px-5 py-3 font-medium">Kayıt Tarihi</th>
          </tr>
        </thead>
        <tbody>
          {loading && (
            <tr><td colSpan={4} className="text-center text-gray-500 py-8">Yükleniyor...</td></tr>
          )}
          {!loading && users.length === 0 && (
            <tr><td colSpan={4} className="text-center text-gray-500 py-8">Kullanıcı yok</td></tr>
          )}
          {users.map(user => (
            <tr
              key={user.id}
              onClick={() => onSelect(user)}
              className="border-b border-gray-800/60 hover:bg-gray-800/50 cursor-pointer transition"
            >
              <td className="px-5 py-3.5">
                <div className="flex items-center gap-3">
                  <div className="w-8 h-8 bg-gray-700 rounded-full flex items-center justify-center text-xs font-bold text-gray-300 flex-shrink-0">
                    {user.name.charAt(0).toUpperCase()}
                  </div>
                  <span className="text-white font-medium">{user.name}</span>
                </div>
              </td>
              <td className="px-5 py-3.5 text-gray-400">{user.email}</td>
              <td className="px-5 py-3.5">
                <span className={`px-2 py-0.5 rounded-md text-xs font-medium ${
                  user.role === 'ADMIN'
                    ? 'bg-purple-500/15 text-purple-400 border border-purple-500/20'
                    : 'bg-blue-500/15 text-blue-400 border border-blue-500/20'
                }`}>
                  {user.role}
                </span>
              </td>
              <td className="px-5 py-3.5 text-gray-500 text-xs">
                {new Date(user.enrolled_at).toLocaleDateString('tr-TR')}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  )
}

function AddUserModal({ adminId, onClose, onSuccess }) {
  const [form, setForm] = useState({ name: '', email: '', password: '', role: 'USER' })
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')

  function set(field, value) {
    setForm(f => ({ ...f, [field]: value }))
  }

  async function handleSubmit(e) {
    e.preventDefault()
    setError('')
    setLoading(true)
    try {
      await api.addUser(form.name, form.email, form.password, form.role, adminId)
      onSuccess()
    } catch (err) {
      setError(err.message)
    } finally {
      setLoading(false)
    }
  }

  return (
    <div
      className="fixed inset-0 bg-black/60 flex items-center justify-center z-50 px-4"
      onClick={onClose}
    >
      <div
        className="bg-gray-900 border border-gray-700 rounded-2xl w-full max-w-md shadow-2xl"
        onClick={e => e.stopPropagation()}
      >
        <div className="flex items-center justify-between px-6 pt-5 pb-4 border-b border-gray-800">
          <h2 className="text-base font-bold text-white">Yeni Kullanıcı</h2>
          <button onClick={onClose} className="text-gray-500 hover:text-gray-300">✕</button>
        </div>
        <form onSubmit={handleSubmit} className="px-6 py-5 space-y-4">
          <Field label="Ad Soyad">
            <input
              type="text" required value={form.name}
              onChange={e => set('name', e.target.value)}
              placeholder="Ahmet Yılmaz"
              className="input"
            />
          </Field>
          <Field label="E-posta">
            <input
              type="email" required value={form.email}
              onChange={e => set('email', e.target.value)}
              placeholder="ahmet@email.com"
              className="input"
            />
          </Field>
          <Field label="Şifre">
            <input
              type="password" required value={form.password}
              onChange={e => set('password', e.target.value)}
              placeholder="••••••••"
              className="input"
            />
          </Field>
          <Field label="Rol">
            <select
              value={form.role}
              onChange={e => set('role', e.target.value)}
              className="input"
            >
              <option value="USER">Kullanıcı</option>
              <option value="ADMIN">Admin</option>
            </select>
          </Field>

          {error && (
            <div className="bg-red-500/10 border border-red-500/30 rounded-lg px-3 py-2.5 text-sm text-red-400">
              {error}
            </div>
          )}

          <div className="flex gap-3 pt-1">
            <button type="button" onClick={onClose}
              className="flex-1 bg-gray-800 hover:bg-gray-700 text-gray-300 py-2.5 rounded-xl text-sm transition">
              İptal
            </button>
            <button type="submit" disabled={loading}
              className="flex-1 bg-blue-600 hover:bg-blue-500 disabled:opacity-60 text-white py-2.5 rounded-xl text-sm font-medium transition">
              {loading ? 'Ekleniyor...' : 'Ekle'}
            </button>
          </div>
        </form>
      </div>
    </div>
  )
}

function Field({ label, children }) {
  return (
    <div>
      <label className="block text-xs font-medium text-gray-400 mb-1.5">{label}</label>
      {children}
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
