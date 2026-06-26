export default function StatusBar({ success, message, elapsedMs }) {
    return (
        <div className={`status-bar ${success ? "status-success" : "status-error"}`}>
            <span className="status-icon">{success ? "✓" : "✗"}</span>
            <span className="status-message">{message}</span>
            <span className="status-time">{elapsedMs?.toFixed(4)} ms</span>
        </div>
    );
}