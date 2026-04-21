import { createContext, useContext, useState } from "react"

const SessionContext = createContext(null)

export function SessionProvider({ children }) {
  const [session, setSession] = useState(() => {
    const stored = localStorage.getItem("session")
    return stored ? JSON.parse(stored) : null
  })

  function login(userData) {
    localStorage.setItem("session", JSON.stringify(userData))
    setSession(userData)
  }

  function logout() {
    localStorage.removeItem("session")
    setSession(null)
  }

  return (
    <SessionContext.Provider value={{ session, login, logout }}>
      {children}
    </SessionContext.Provider>
  )
}

export function useSession() {
  return useContext(SessionContext)
}
