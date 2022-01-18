// Query valori di particolato minimo, medio, massimo
// aggregati per giorno
//

// Query config
date_start   = "2021-12-31T00:00:00.0Z"
date_stop    = "2022-01-14T23:00:00.0Z"
stream_name1 = "dati"

// Concentrazione PM10 per massa
field_name  = "mass_of_pm10"

// Concentrazione PM2,5 per massa
// field_name  = "mass_of_pm2"

// Genera uno stream con campionamento ogni ora
//
data1 = from(bucket: "test")
  |> range(start: time(v: date_start), stop: time(v: date_stop))
  |> filter(fn: (r) => r["_measurement"] == "airq_monitor_tambone")
  |> filter(fn: (r) => r["_field"] ==  field_name)
  |> aggregateWindow(every: 1h, fn: last, createEmpty: false)
  |> yield(name: stream_name1)

// min, med, max aggregato sulle 24h
//
minimo = data1
  |> aggregateWindow(every: 24h, fn: min, createEmpty: false)
  |> yield(name: "minimo")

medio = data1
  |> aggregateWindow(every: 24h, fn: mean, createEmpty: false)
  |> yield(name: "medio")

massimo = data1
  |> aggregateWindow(every: 24h, fn: max, createEmpty: false)
  |> yield(name: "massimo")

