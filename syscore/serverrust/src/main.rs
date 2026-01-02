mod metrics;
mod apps;

use actix_cors::Cors;
use actix_web::{get, post, middleware::Logger, web, App, HttpResponse, HttpServer, Responder};
use serde_json::json;

const SERVER_VERSION: &str = env!("CARGO_PKG_VERSION");

#[get("/health")]
async fn health_handler() -> impl Responder {
    HttpResponse::Ok().json(json!({ "status": "ok" }))
}

#[get("/version")]
async fn version_handler() -> impl Responder {
    HttpResponse::Ok().json(json!({
        "version": SERVER_VERSION,
        "commit_hash": option_env!("GIT_COMMIT_HASH").unwrap_or("n/a"),
        "build_time": option_env!("BUILD_TIMESTAMP").unwrap_or("n/a"),
    }))
}

async fn metrics_handler() -> impl Responder {
    match metrics::collect_metrics() {
        Ok(metrics) => HttpResponse::Ok().json(metrics),
        Err(err) => HttpResponse::InternalServerError().json(json!({
            "error": err.to_string()
        })),
    }
}

#[get("/apps")]
async fn apps_handler() -> impl Responder {
    match apps::scan_installed_programs() {
        Ok(apps_response) => {
            HttpResponse::Ok()
                .content_type("application/json; charset=utf-8")
                .json(apps_response)
        }
        Err(err) => {
            log::error!("Failed to scan installed programs: {}", err);
            HttpResponse::InternalServerError().json(json!({
                "error": err.to_string()
            }))
        }
    }
}

#[post("/get-icon")]
async fn get_icon_handler(form: web::Form<IconRequest>) -> impl Responder {
    let path = form.path.as_str();
    
    if path.is_empty() {
        return HttpResponse::BadRequest().json(json!({
            "error": "path is required"
        }));
    }

    match apps::extract_icon_base64(path) {
        Ok(base64_icon) => {
            HttpResponse::Ok()
                .content_type("text/plain; charset=utf-8")
                .body(base64_icon)
        }
        Err(err) => {
            log::error!("Failed to extract icon from {}: {}", path, err);
            HttpResponse::InternalServerError().json(json!({
                "error": err.to_string()
            }))
        }
    }
}

#[derive(serde::Deserialize)]
struct IconRequest {
    path: String,
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    env_logger::init();

    HttpServer::new(move || {
        App::new()
            .wrap(Logger::default())
            .wrap(
                Cors::default()
                    .allow_any_origin()
                    .allow_any_method()
                    .allow_any_header(),
            )
            .service(health_handler)
            .service(version_handler)
            .service(apps_handler)
            .service(get_icon_handler)
            .route("/metrics", web::get().to(metrics_handler))
    })
    .bind(("0.0.0.0", 7148))?
    .run()
    .await
}
