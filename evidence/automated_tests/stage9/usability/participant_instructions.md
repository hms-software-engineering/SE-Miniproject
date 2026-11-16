# HMS receptionist usability session — participant sheet

## For the participant

Please use the HMS application as you normally would. You will receive two tasks: register the supplied synthetic patient, then book an appointment for that patient. Work without assistance unless you become unable to continue. You may ask to stop at any time. This session uses a disposable test environment and fictional data; do not enter real patient information.

## For the moderator (do not read the steps aloud)

Use one neutral participant code U01–U05. Confirm the application is connected to the isolated usability test environment and reset it before each participant. Give the participant one matching row from `dummy_patient_data.csv`. Start the registration timer when the participant begins interacting; stop when the application displays the saved patient ID. Then give this prompt: “Please book an appointment for this patient with the requested doctor on the requested date.” Do not coach, point, or interpret labels. If asked for help, record the request and any assistance before responding. Record success, elapsed time, errors, and assistance in the accompanying blank CSV sheets. Do not record participant names, screenshots of personal data, or any real patient information.

## Standardization and pass rule

Use the same app build, display resolution, test account role, network/database setup, and task wording for all five sessions. Reset the disposable database state between sessions and verify the calendar date/doctor slot is available. Each participant performs two core tasks (registration and appointment booking), for ten total tasks. The assigned procedure passes only if all five registrations finish within 180 seconds and at least 9/10 core tasks complete unaided. “Unaided” means no moderator hint, correction, or navigation assistance. A timeout or participant stop is recorded as incomplete, never silently removed from the denominator.

## Synthetic data handling

The phone numbers below are syntactically valid dummy values and must be used only in the isolated test database; the app has no contact-delivery workflow in this procedure. Never call or message them. Use the listed patient row only for its matching participant and remove/reset the disposable test database after the study according to the local test-data policy.
