function normalizeLabel(value) {
  return String(value || '')
    .toLowerCase()
    .normalize('NFD')
    .replace(/[\u0300-\u036f]/g, '')
    .replace(/[\u0131\u0130]/g, 'i')
    .replace(/[\u015F\u015E]/g, 's')
    .replace(/[\u011F\u011E]/g, 'g')
    .replace(/[\u00FC\u00DC]/g, 'u')
    .replace(/[\u00F6\u00D6]/g, 'o')
    .replace(/[\u00E7\u00C7]/g, 'c')
}

export function relativeTime(dateStr) {
  const diff = (Date.now() - new Date(dateStr)) / 1000
  if (diff < 60) return 'Just now'
  if (diff < 3600) return `${Math.floor(diff / 60)} min ago`
  if (diff < 86400) return `${Math.floor(diff / 3600)} hr ago`
  return `${Math.floor(diff / 86400)} days ago`
}

export function formatDateTime(dateStr, options) {
  return new Date(dateStr).toLocaleString('en-US', options)
}

export function formatDate(dateStr, options) {
  return new Date(dateStr).toLocaleDateString('en-US', options)
}

export function formatStatus(status) {
  const labels = {
    'hatali parmak izi': 'Failed fingerprint',
    failed: 'Failed',
    unlocked: 'Unlocked',
    lockout: 'Lockout',
    locked: 'Locked',
    'no activity': 'No activity',
  }

  return labels[normalizeLabel(status)] || status || '-'
}
