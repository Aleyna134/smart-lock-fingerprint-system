import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom'
import { useSession } from './context/SessionContext'
import LoginPage from './pages/LoginPage'
import Layout from './components/Layout'
import DashboardPage from './pages/DashboardPage'
import AccessLogsPage from './pages/AccessLogsPage'
import UsersPage from './pages/UsersPage'
import AlertsPage from './pages/AlertsPage'
import SettingsPage from './pages/SettingsPage'

function PrivateRoute({ children }) {
  const { session } = useSession()
  return session ? children : <Navigate to="/login" replace />
}

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/login" element={<LoginPage />} />
        <Route
          path="/"
          element={
            <PrivateRoute>
              <Layout />
            </PrivateRoute>
          }
        >
          <Route index element={<Navigate to="/dashboard" replace />} />
          <Route path="dashboard" element={<DashboardPage />} />
          <Route path="logs" element={<AccessLogsPage />} />
          <Route path="users" element={<UsersPage />} />
          <Route path="alerts" element={<AlertsPage />} />
          <Route path="settings" element={<SettingsPage />} />
        </Route>
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </BrowserRouter>
  )
}
