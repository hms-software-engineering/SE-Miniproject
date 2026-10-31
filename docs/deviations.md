# F-024 — Doctor Daily Appointment Schedule Deviations

1. Cancelled appointments are excluded from the daily schedule.
2. The response uses a numeric appointmentId, while the handover example specifies a string.
3. The slotStart value uses the format "YYYY-MM-DD HH:MM", while the handover example uses a "T" separator.
4. The response includes doctorId in addition to the other appointment fields.

These differences should be reviewed against the expected API response contract.

## F-042 – Payment Receipt Generation

- The SAD defines no receipt-retrieval endpoint. F-042 adds `GET /api/v1/payments/{paymentId}/receipt`, returning print-ready HTML. It follows F-041’s flat `/api/v1/payments` route, which already differs from the SAD’s `POST /api/v1/bills/{id}/payments`.
- Receipt dates are shown in IST (`DD-MM-YYYY HH:MM`), converted from the stored `paid_at` TIMESTAMPTZ.
- Receipt balance is computed as of that payment, so a reprint always shows the original figure.

## F-061 – Administrator Audit Log Filtering

**1. Audit API response format**

- The Software Architecture and Design specification describes the `GET /api/v1/audit` response as a JSON array of audit entries.
- The implementation returns a JSON object containing an `entries` array:
  ```json
  {
    "entries": [
      {
        "userId": 18,
        "username": "dr.rao",
        "role": "Doctor",
        "action": "LOGIN",
        "recordId": "18",
        "timestamp": "2026-10-30T06:27:53.884626Z",
        "sourceIp": "127.0.0.1"
      }
    ]
  }